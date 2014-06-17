#pragma once

#include <string>
#include <array>
#include <vector>
#include <map>

#include <yaml-cpp/yaml.h>
#include <sndfile.h>
#include <portmidi.h>

#include "channel.h"


class Server {
public:
	enum { MIXRATE = 44100 };


	Server();
	~Server();

	void initialize(const YAML::Node& n);

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
	int _midi_channel_nr;


	// data linked to the song
	YAML::Node _root;
	std::map<std::string, std::vector<Channel::Command>> _instruments;
	int _ticks_per_row;
	int _frames_per_tick;

	// position
	int _frame;
	int _tick;
	int _row;
	int _block;

	std::array<Channel, 16> _channels;
};

