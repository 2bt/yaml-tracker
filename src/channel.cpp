#include <iostream>
#include <vector>
#include <stdexcept>
#include <cmath>
#include <map>

#include "util.h"
#include "channel.h"
using namespace std;

const vector<Channel::Command> Channel::get_default_instrument() {
	return {
		{ "wave",			"PULSE" },
		{ "offset",			"0" },
		{ "pulsewidth",		"0.5" },
		{ "pulsewidth~",	"0" },
		{ "panning",		"0" },
		{ "volume",			"1" },
		{ "resolution",		"0" },
		{ "vibrato-speed",	"0.1" },
		{ "vibrato-depth",	"0" },
		{ "gliss",			"0" },

		{ "filter",			"OFF" },
		{ "filter-freq",	"2000" },
		{ "filter-freq~",	"0" },
		{ "filter-reso",	"1" },

		{ "attack",			"0.002" },
		{ "decay",			"0.99992" },
		{ "sustain",		"0.5" },
		{ "release",		"0.999" },
	};
}

void Channel::initialize() {

	_shift = 0x7ffff8;
	_state = State::Off;

	CmdExecState ces(get_default_instrument());
	exec_commands(ces);

	_filter.initialize();
	_filter.set(_filter_freq, _filter_reso);


}

void Channel::tick(const map<string, vector<Command>>& instruments) {
	exec_commands(_inst_ces, instruments);
	exec_commands(_row_ces, instruments);



	// pulse width
	_pulsewidth = fmodf(_pulsewidth + _pulsewidth_sweep, 1);

	// filter
	if (_filter_freq_sweep != 0) {
		_filter_freq += _filter_freq_sweep;
		if (_filter_freq < 10) _filter_freq = 10;
		_filter.set(_filter_freq, _filter_reso);
	}

	// glissando
	if (_gliss > 0) {
		if (_note < _note_dst) {
			_note += _gliss;
			if (_note > _note_dst) _note = _note_dst;
		}
		else {
			_note -= _gliss;
			if (_note < _note_dst) _note = _note_dst;
		}
	}

	// vibrato
	_vibrato_phase = fmodf(_vibrato_phase + _vibrato_speed, 1);
	float vib = sinf(_vibrato_phase * 2 * M_PI) * _vibrato_depth;


	// calculate osc speed
	_speed = powf(2, (_note + _offset + vib - 57) * (1 / 12.0)) * (440.0 / 44100);
}


void Channel::exec_commands(CmdExecState& ces,
		const map<string, vector<Command>>& instruments) {

	for (;;) {
		if (ces.wait > 0) {
			ces.wait--;
			return;
		}

		if (ces.pos >= (int) ces.cmds.size()) return;
		Command& cmd = ces.cmds[ces.pos++];

		if (cmd.name == "note") {
			const string& n = cmd.value;
			if (n == "---") _state = State::Release;
			else {
				bool gliss = n.size() == 4 && n[3] == '*';
				if ((n.size() != 3 && !gliss) ||
					n[0] < 'a' || n[0] > 'g' ||
					(n[1] != '-' && n[1] != '#') ||
					n[2] < '0' || n[2] > '9') {
					throw logic_error("error interpreting note: " + n);
				}
				_note_dst = string("ccddeffggaab").find(n[0]);
				_note_dst += n[1] == '#';
				_note_dst += (n[2] - '0') * 12;

				if (gliss) {
					if (_gliss <= 0) _note = _note_dst;
				}
				else {
					_note = _note_dst;
					_state = State::Attack;
					_level = 0;
					_phase = 0;
					_vibrato_phase = 0;
				}

				if (!_instrument.empty()) {
					_inst_ces.set(instruments.at(_instrument));
					exec_commands(_inst_ces, instruments);
				}
			}
		}

		else if (cmd.name == "inst" || cmd.name == "i") {
			if (!instruments.count(cmd.value)) {
				throw logic_error("unknown instrument: " + cmd.value);
			}
			_instrument = cmd.value;
			_inst_ces.set(instruments.at(_instrument));
			exec_commands(_inst_ces, instruments);
		}
		else if (cmd.name == "call") {
			if (!instruments.count(cmd.value)) {
				throw logic_error("unknown instrument: " + cmd.value);
			}
			CmdExecState c(instruments.at(cmd.value));
			exec_commands(c, instruments);
		}
		else if (cmd.name == "attack") {
			_attack = strtofloat(cmd.value);
		}
		else if (cmd.name == "decay") {
			_decay = strtofloat(cmd.value);
		}
		else if (cmd.name == "sustain") {
			_sustain = strtofloat(cmd.value);
		}
		else if (cmd.name == "release") {
			_release = strtofloat(cmd.value);
		}
		else if (cmd.name == "volume" || cmd.name == "v") {
			_volume = strtofloat(cmd.value);
		}
		else if (cmd.name == "panning" || cmd.name == "p") {
			float p = strtofloat(cmd.value);
			_panning[0] = sqrtf(0.5 - p * 0.5);
			_panning[1] = sqrtf(0.5 + p * 0.5);
		}
		else if (cmd.name == "offset" || cmd.name == "o") {
			_offset = strtofloat(cmd.value);
		}
		else if (cmd.name == "offset+" || cmd.name == "o+") {
			_offset += strtofloat(cmd.value);
		}
		else if (cmd.name == "pulsewidth" || cmd.name == "pw") {
			_pulsewidth = strtofloat(cmd.value);
		}
		else if (cmd.name == "pulsewidth~" || cmd.name == "pw~") {
			_pulsewidth_sweep = strtofloat(cmd.value);
		}
		else if (cmd.name == "vibrato-speed" || cmd.name == "vs") {
			_vibrato_speed = strtofloat(cmd.value);
		}
		else if (cmd.name == "vibrato-depth" || cmd.name == "vd") {
			_vibrato_depth = strtofloat(cmd.value);
		}
		else if (cmd.name == "gliss") {
			_gliss = strtofloat(cmd.value);
		}
		else if (cmd.name == "resolution") {
			_resolution = strtoint(cmd.value);
		}
		else if (cmd.name == "wave" || cmd.name == "w") {
			try {
				_wave = map<string, Wave> {
					{ "PULSE",		Wave::Pulse },
					{ "TRIANGLE",	Wave::Triangle },
					{ "C64NOISE",	Wave::C64Noise },
					{ "NOISE",		Wave::Noise },
					{ "SINE",		Wave::Sine }
				}.at(cmd.value);
			}
			catch (const out_of_range& e) {
				cout << "<" << cmd.value << ">" << endl;
				throw logic_error("error parsing wave: " + cmd.name);
			}
		}
		else if (cmd.name == "filter") {
			if (cmd.value == "ON") _filter_active = true;
			else if (cmd.value == "OFF") _filter_active = false;
			else throw logic_error("error parsing filter: " + cmd.name);
		}
		else if (cmd.name == "filter-freq") {
			_filter_freq = strtofloat(cmd.value);
			_filter.set(_filter_freq, _filter_reso);
		}
		else if (cmd.name == "filter-freq+") {
			_filter_freq += strtofloat(cmd.value);
			if (_filter_freq < 10) _filter_freq = 10;
			_filter.set(_filter_freq, _filter_reso);
		}
		else if (cmd.name == "filter-freq~") {
			_filter_freq_sweep = strtofloat(cmd.value);
		}
		else if (cmd.name == "filter-reso") {
			_filter_reso = strtofloat(cmd.value);
			_filter.set(_filter_freq, _filter_reso);
		}
		else if (cmd.name == "loop-count") {
			ces.loop_count = strtoint(cmd.value);
		}
		else if (cmd.name == "loop") {
			if (ces.loop_count != 0) {
				if (ces.loop_count > 0) ces.loop_count--;

				int v = strtoint(cmd.value);
				if (v >= 0) ces.pos = v;
				else ces.pos = max(0, ces.pos + v - 1);
			}
		}
		else if (cmd.name == "wait" || cmd.name == "") {
			ces.wait = strtoint(cmd.value);
		}
		else {
			throw logic_error("unknown command: " + cmd.name);
		}

	}

}


void Channel::addMix(float frame[2]) {

	switch (_state) {
	case State::Off: return;
	case State::Attack:
		_level += _attack;
		if (_level > 1) {
			_level = 1;
			_state = State::Hold;
		}
		break;
	case State::Hold:
		_level = _sustain + (_level - _sustain) * _decay;
		break;
	case State::Release:
	default:
		_level *= _release;
		break;
	}


	_phase += _speed;
	if (_wave != Wave::C64Noise) _phase = fmodf(_phase, 1);

	float amp = 0;

	switch (_wave) {
	case Wave::Pulse:
		amp = _phase < _pulsewidth ? -1 : 1;
		break;
	case Wave::Triangle:
		amp = _phase < _pulsewidth ?
			2 / _pulsewidth * _phase - 1 :
			2 / (_pulsewidth - 1) * (_phase - _pulsewidth) + 1;
		break;
	case Wave::Sine:
		amp = sinf(_phase * 2 * M_PI);
		break;
	case Wave::Noise:
		amp = rand() / float(RAND_MAX) * 2 - 1;
		break;
	case Wave::C64Noise: {
		unsigned int s = _shift;
		unsigned int b;
		while (_phase > 0.1) {
			_phase -= 0.1;
			b = ((s >> 22) ^ (s >> 17)) & 1;
			s = ((s << 1) & 0x7fffff) + b;
		}
		_shift = s;
		amp = (
			((s & 0x400000) >> 11) |
			((s & 0x100000) >> 10) |
			((s & 0x010000) >> 7) |
			((s & 0x002000) >> 5) |
			((s & 0x000800) >> 4) |
			((s & 0x000080) >> 1) |
			((s & 0x000010) << 1) |
			((s & 0x000004) << 2)) * (2.0 / (1<<12)) - 1;
		break;
	}
	default: break;
	}

	amp *= _level;

	if (_resolution > 0) {
		amp = int((amp + 1) * _resolution) / float(_resolution) - 1;
	}

	if (_filter_active) amp = _filter.mix(amp);

	amp *= _volume;

	frame[0] += amp * _panning[0];
	frame[1] += amp * _panning[1];
}

