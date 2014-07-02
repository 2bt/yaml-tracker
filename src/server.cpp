#include <SDL/SDL.h>
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
	if (_log) sf_close(_log);
	if (_midi) Pm_Close(_midi);
	Pm_Terminate();
	SDL_CloseAudio();
}




vector<Channel::Command> get_commands(const string& row) {
	vector<Channel::Command> cmds;

	size_t p = row.find_first_not_of(" \n\t");
	while (p != string::npos) {
		Channel::Command cmd = { "note", "" };

		size_t pos = row.find_first_of(" \n\t:", p);
		if (pos < row.length() - 1 && row[pos] == ':') {
			cmd.name = row.substr(p, pos - p);
			p = row.find_first_not_of(" \n\t", pos + 1);
			pos = row.find_first_of(" \n\t", p);
		}
		if (cmd.name[0] != '#') {
			cmd.value = row.substr(p, pos - p);
			cmds.emplace_back(cmd);
		}

		p = row.find_first_not_of(" \n\t", pos);
	}
	return cmds;
}


void Server::initialize(const YAML::Node& n) {
	Lock lock;

	_root = n;

	_instruments = { { "default", Channel::get_default_instrument() } };
	for (const auto& pair : _root["instruments"]) {
		_instruments[pair.first.as<string>()] =
			get_commands(pair.second.as<string>());
	}

	_ticks_per_row = _root["ticks"] ? _root["ticks"].as<int>() : 8;
	_frames_per_tick = _root["frames"] ? _root["frames"].as<int>() : 800;
	_midi_channel_nr = _root["midi"] ? strtoint(_root["midi"].as<string>())
									 : _channels.size() - 1;
	if (_instruments.count("midi")) _channels[_midi_channel_nr].set_midi_instrument();

	_frame = 0;
	_tick = 0;
	_row = 0;
	_block = 0;
	for (auto& chan : _channels) chan.initialize();

	_playing = true;
}



void Server::tick() {
	YAML::Node tr = _root["table"][_block];

	for (int i = 0; i < int(_channels.size()); i++) {

		if (_tick == 0 && i < int(tr.size()) && !tr[i].IsNull()) {
			string pat = tr[i].as<string>();
			if (!_root["patterns"][pat]) throw logic_error("undefined pattern: " + pat);
			YAML::Node row = _root["patterns"][pat][_row];
			if (row.IsScalar()) {
				auto cmds = get_commands(row.as<string>());
				_channels[i].set_row_commands(cmds);
			}
			else if (row.IsSequence()) {
				for (int j = 0; j < row.size(); j++) {
					auto cmds = get_commands(row[j].as<string>());
					_channels[(i + j) % _channels.size()].set_row_commands(cmds);
				}
			}
		}

		// midi
		if (_midi && i == _midi_channel_nr) {
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

		_channels[i].tick(_instruments);
	}
}


void Server::mix(short* buffer, int length) {
	for (int i = 0; i < length; i += 2) {

		if (_playing) {
			try {
				if (_frame == 0) tick();

				if (++_frame >= _frames_per_tick) {
					_frame = 0;
					if (++_tick >= _ticks_per_row) {
						_tick = 0;

						if (_root["table"]) {
							// take the length of the first pattern in the table row
							string pat = _root["table"][_block][0].as<string>();
							int pat_len = int(_root["patterns"][pat].size());
							if (++_row >= pat_len) {
								_row = 0;
								if (++_block >= int(_root["table"].size())) _block = 0;
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
		buffer[i + 0] = frame[0] * 4000;
		buffer[i + 1] = frame[1] * 4000;
	}

	sf_writef_short(_log, buffer, length / 2);
}


void Server::recover() {
	_playing = false;
	for (auto& chan : _channels) chan.release();
}
