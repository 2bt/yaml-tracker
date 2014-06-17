#pragma once

#include <vector>
#include <string>

class Channel {
public:
	struct Command {
		std::string name;
		std::string value;
	};

	Channel() {}
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
	enum class Wave { Pulse, Triangle, Sine, Noise };


	State			_state;
	float			_level;
	float			_phase;
	float			_speed;
	unsigned int	_shift;


	int				_note;
	float			_offset;
	float			_volume;
	float			_panning[2];

	float			_attack;
	float			_decay;
	float			_sustain;
	float			_release;

	float			_pulsewidth;
	float			_pulsesweep;
	float			_vibrato_phase;
	float			_vibrato_speed;
	float			_vibrato_depth;
	Wave			_wave;

	std::string		_instrument;


	struct CmdExecState {
		std::vector<Command>	cmds;
		size_t					pos;
		int						wait;

		void set(std::vector<Command> c) {
			cmds = c;
			pos = 0;
			wait = 0;
		}
	};
	CmdExecState	_inst_ces;
	CmdExecState	_row_ces;

	void exec_commands(CmdExecState& ces, const std::map<std::string, std::vector<Command>>& instruments = {});

};

