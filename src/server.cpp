#include <SDL/SDL.h>
#include <iostream>
#include "util.h"
#include "server.h"


using namespace std;

Server::Lock::Lock() { SDL_LockAudio(); }
Server::Lock::~Lock() { SDL_UnlockAudio(); }


Server::Server() {

	// open log file
	SF_INFO info = { 0, MIXRATE, 2, SF_FORMAT_WAV | SF_FORMAT_PCM_16 };
	_log = sf_open("log.wav", SFM_WRITE, &info);
	if (!_log) cerr << "error opening log\n";


	// find midi device
	Pm_Initialize();
	_midi = nullptr;
	int dev_count = Pm_CountDevices();
	int i;
	for (i = 0; i < dev_count; i++) {
		const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
		if (info->output &&
			string(info->name).find("MIDI") != string::npos) break;
	}
	if (i < dev_count) {
		Pm_OpenInput(&_midi, 3, nullptr, 0, nullptr, nullptr);
	}

	// start sound server
	static SDL_AudioSpec spec = { MIXRATE, AUDIO_S16SYS,
		2, 0, 1024, 0, 0, &Server::audio_callback, this
	};
	SDL_OpenAudio(&spec, &spec);
	SDL_PauseAudio(0);

}
Server::~Server() {
	SDL_CloseAudio();
	if (_log) sf_close(_log);
	if (_midi) Pm_Close(_midi);
	Pm_Terminate();
}




static vector<Channel::Command> get_commands(const string& s) {
	vector<Channel::Command> cmds;
	size_t p = s.find_first_not_of(" \n\t");
	while (p != string::npos) {
		Channel::Command cmd = { "note", "" };

		size_t pos = s.find_first_of(" \n\t:", p);
		if (pos < s.length() - 1 && s[pos] == ':') {
			cmd.name = s.substr(p, pos - p);
			p = s.find_first_not_of(" \n\t", pos + 1);
			pos = s.find_first_of(" \n\t", p);
		}
		if (cmd.name[0] != '#') {
			cmd.value = s.substr(p, pos - p);
			cmds.emplace_back(cmd);
		}

		p = s.find_first_not_of(" \n\t", pos);
	}
	return cmds;
}


static vector<vector<Channel::Command>> get_multi_commands(const string& s) {
	vector<vector<Channel::Command>> cmds;
	size_t p1 = 0;
	size_t p2 = 0;
	while (p2 != string::npos) {
		p2 = s.find('/', p1);
		cmds.push_back(get_commands(s.substr(p1, p2 - p1)));
		p1 = p2 + 1;
	}
	return cmds;
}


void TuneData::initialize(const YAML::Node& n) {
	table = n["table"];
	patterns = n["patterns"];

	instruments = { { "default", Channel::get_default_instrument() } };
	for (const auto& pair : n["instruments"]) {
		instruments[pair.first.as<string>()] =
			get_commands(pair.second.as<string>());
	}

	ticks_per_row = n["ticks"] ? n["ticks"].as<int>() : 8;
	frames_per_tick = n["frames"] ? n["frames"].as<int>() : 800;
	midi_channel_nr = n["midi"] ? strtoint(n["midi"].as<string>())
									 : Server::CHANNEL_COUNT - 1;
}


void Server::reinitialize(const YAML::Node& n) {
	if (_playing == false || !_tune.table.IsDefined()) {
		initialize(n);
		return;
	}

	_new_tune.initialize(n);
	_switch_tune = true;
}


void Server::initialize(const YAML::Node& n) {
	Lock lock;

	_tune.initialize(n);
	if (_tune.instruments.count("midi")) _channels[_tune.midi_channel_nr].set_midi_instrument();

	_frame = 0;
	_tick = 0;
	_row = 0;
	_block = 0;
	for (auto& chan : _channels) chan.initialize();

	_playing = true;
}



void Server::tick() {
	YAML::Node tr = _tune.table[_block];

	for (int i = 0; i < int(_channels.size()); i++) {

		if (_tick == 0 && i < int(tr.size()) && !tr[i].IsNull()) {
			string pat = tr[i].as<string>();
			if (!_tune.patterns[pat]) throw logic_error("undefined pattern: " + pat);
			YAML::Node row = _tune.patterns[pat][_row];
			if (row.IsScalar()) {
				auto cmds = get_multi_commands(row.as<string>());
				int l = min(cmds.size(), _channels.size() - i + cmds.size());
				for (int j = 0; j < l; j++) _channels[i + j].set_row_commands(cmds[j]);
			}
		}


		// midi
		if (_midi && i == _tune.midi_channel_nr) {
			struct { unsigned char type, val, x, y; } event;
			for (;;) {
				int l = Pm_Read(_midi, (PmEvent*) &event, 1);
				if (!l) break;

				static int last_note = 0;
				string row;
				if (event.type == 128 && event.val == last_note) row = "---";
				else if (event.type == 144) {
					int i = event.val;
					row	= string(1, "ccddeffggaab"[i%12])
						+ string(1, "-#-#--#-#-#-"[i%12])
						+ string(1, '0' + i/12);
					last_note = event.val;
				}
				if (!row.empty()) _channels[i].set_row_commands({ { "note", row } });

			}
		}

		_channels[i].tick(_tune.instruments);
	}
}

static inline short clamp_int_to_short(int x) {
	x = x < -32768 ? -32768 : x;
	return x > 32767 ? 32767 : x;
}

void Server::mix(short* buffer, int length) {
	for (int i = 0; i < length; i += 2) {

		if (_playing) {
			try {
				if (_frame == 0) tick();

				if (++_frame >= _tune.frames_per_tick) {
					_frame = 0;
					if (++_tick >= _tune.ticks_per_row) {
						_tick = 0;

						if (_tune.table) {
							// take the length of the first pattern in the table row
							string pat = _tune.table[_block][0].as<string>();
							int pat_len = int(_tune.patterns[pat].size());
							if (++_row >= pat_len) {
								_row = 0;

								if (_switch_tune) {
									_switch_tune = false;
									_tune = _new_tune;
									if (_tune.instruments.count("midi")) _channels[_tune.midi_channel_nr].set_midi_instrument();
									_block = 0;
								}
								else {
									if (++_block >= int(_tune.table.size())) _block = 0;
								}
							}
						}
					}
				}
			}

			catch (const YAML::Exception& e) {
				cout << "Server: invalid data structure" << endl;
				recover();
				goto DONE;

			}
			catch (const logic_error& e) {
				cout << "Server: " << e.what() << endl;
				recover();
				goto DONE;
			}

		}
DONE:

		float frame[2] = { 0, 0 };
		for (auto& chan : _channels) chan.addMix(frame);
		buffer[i + 0] = clamp_int_to_short(frame[0] * 6000);
		buffer[i + 1] = clamp_int_to_short(frame[1] * 6000);
	}

	sf_writef_short(_log, buffer, length / 2);
}


void Server::recover() {
	_playing = false;
	for (auto& chan : _channels) chan.release();
}
