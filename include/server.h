#pragma once

#include <string>
#include <array>
#include <vector>
#include <map>

#include <yaml-cpp/yaml.h>
#include <sndfile.h>
#include <portmidi.h>

#include "channel.h"


struct TuneData {
	void initialize(const YAML::Node& n);

	YAML::Node table;
	YAML::Node patterns;
	std::map<std::string, std::vector<Channel::Command>> instruments;
	int ticks_per_row;
	int frames_per_tick;
	int midi_channel_nr;
};


class Server {
public:
	enum {
		MIXRATE = 44100,
		CHANNEL_COUNT = 16
	};


	Server();
	~Server();

	void initialize(const YAML::Node& n);
	void reinitialize(const YAML::Node& n);

private:
	struct Lock { Lock(); ~Lock(); };

	static void audio_callback(void* userdata, unsigned char* stream, int len) {
		((Server*) userdata)->mix((short*) stream, len / 2);
	}

	void mix(short* buffer, int length);
	void tick();
	void recover();


	bool _playing = false;

	SNDFILE* _log;
	PortMidiStream*	_midi;


	TuneData _tune;
	TuneData _new_tune;
	bool _switch_tune = false;

	// position
	int _frame;
	int _tick;
	int _row;
	int _block;

	std::array<Channel, CHANNEL_COUNT> _channels;
};

