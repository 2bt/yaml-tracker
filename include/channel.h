#pragma once

#include <vector>
#include <string>
#include <cmath>


class LPF {
public:
	void initialize() {
		_pos = 0;
		_speed = 0;
	}

	void set(float freq, float reso) {
		float w = 2 * M_PI * freq / 44100.0;
		float q = 1 - w / (2 * (reso + 0.5 / (1 + w)) + w - 2);
		_r = q * q;
		_c = _r + 1 - 2 * cosf(w) * q;
	}
	float mix(float a) {
		_speed += (a - _pos) * _c;
		_pos += _speed;
		_speed *= _r;
		return _pos;
	}

private:
	float _r;
	float _c;

	float _pos;
	float _speed;
};


class Channel {
public:
	struct Command {
		std::string name;
		std::string value;
	};
	static const std::vector<Command> get_default_instrument();

	Channel() {
		_level = 0;
	}
	void initialize();
	void addMix(float frame[2]);
	void tick(const std::map<std::string, std::vector<Command>>& instruments);


	void set_row_commands(std::vector<Command> c) {
		_row_ces.set(c);
	}

	void release() {
		if (_state != State::Off) _state = State::Release;
	}
	void set_midi_instrument() {
		_instrument = "midi";
	}

private:
	enum class State { Off, Release, Attack, Hold };
	enum class Wave { Pulse, Triangle, Sine, Noise, C64Noise };


	State			_state;
	float			_level;
	float			_phase;
	float			_speed;
	unsigned int	_shift;


	float			_note;
	float			_note_dst;
	float			_gliss;

	float			_offset;
	float			_volume;
	float			_panning[2];

	float			_attack;
	float			_decay;
	float			_sustain;
	float			_release;

	float			_pulsewidth;
	float			_pulsewidth_sweep;
	float			_vibrato_phase;
	float			_vibrato_speed;
	float			_vibrato_depth;
	int				_resolution;
	Wave			_wave;

	bool			_filter_active;
	float			_filter_freq;
	float			_filter_freq_sweep;
	float			_filter_reso;
	LPF				_filter;

	std::string		_instrument;


	struct CmdExecState {
		std::vector<Command>	cmds;
		int						pos;
		int						wait;
		int						loop_count;

		CmdExecState(const std::vector<Command>& c={}) {
			set(c);
		}
		void set(std::vector<Command> c) {
			cmds = c;
			pos = 0;
			wait = 0;
			loop_count = -1;
		}
	};
	CmdExecState	_inst_ces;
	CmdExecState	_row_ces;

	void exec_commands(CmdExecState& ces,
		const std::map<std::string, std::vector<Command>>& instruments = {});

};

