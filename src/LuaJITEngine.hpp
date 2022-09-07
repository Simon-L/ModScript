// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021 Modifications by Simon-L

#pragma once
#include <rack.hpp>
#include "plugin.hpp"
#include <luajit-2.0/lua.hpp>

static const int NUM_ROWS = 4;
static const int MAX_BUFFER_SIZE = 4096;
static const int MAX_MIDI_MESSAGES = 1024;
static const int MAX_CABLES = 255;
static const int MAX_SYSEX_LEN = 2048;

struct Lune;

struct ProcessBlock {
	float sampleRate = 0.f;
	float sampleTime = 0.f;
	int bufferSize = 1;
	int midiInputSize = 0;
	float inputs[NUM_ROWS][MAX_BUFFER_SIZE] = {};
	float outputs[NUM_ROWS][MAX_BUFFER_SIZE] = {};
	uint8_t midiInput[MAX_MIDI_MESSAGES][4] = {};
	float knobs[2] = {};
	float light[3] = {};
	bool button;
};


struct LuaJITEngine {
	// Virtual methods for subclasses
	virtual ~LuaJITEngine();
	virtual std::string getEngineName();
	/** Executes the script.
	Return nonzero if failure, and set error message with setMessage().
	Called only once per instance.
	*/
	virtual int run(const std::string& path, const std::string& script);

	/** Calls the script's process() method.
	Return nonzero if failure, and set error message with setMessage().
	*/
	virtual int process();

	midi::Message sysexMessage;

	std::vector<uint8_t> sysexData;

	// This is a mirror of ProcessBlock that we are going to use
	// to provide 1-based indices within the Lua VM
	struct LuaProcessBlock {
		float sampleRate;
		float sampleTime;
		int bufferSize;
		int midiInputSize;
		float* inputs[NUM_ROWS + 1];
		float* outputs[NUM_ROWS + 1];
		uint8_t* midiInput[MAX_MIDI_MESSAGES + 1];
		float* knobs;
		float* light;
		bool button;
	};

	LuaProcessBlock luaBlock;

	// Communication with Prototype module.
	// These cannot be called from your constructor, so initialize your engine in the run() method.
	void display(const std::string& message);
	void setParamValue(const int64_t moduleId, const int paramId, const double paramValue, const bool normalized, const bool noIndicator, const bool relative);
	double getParamValue(const int64_t moduleId, const int paramId);
	double getLightValue(const int64_t moduleId, const int lightId);
	int64_t addCable(const int64_t outputModuleId, const int outputId, const int64_t inputModuleId, const int inputId);
	bool removeCable(const int64_t cableId);
	bool sendMidiMessage(const uint8_t status, const uint8_t channel, const uint8_t note, const uint8_t value);
	bool sendSysexMessage(const uint16_t size);
	void setFrameDivider(int frameDivider);
	void setBufferSize(int bufferSize);
	ProcessBlock* getProcessBlock();
	static int native_display(lua_State* L);
	static int native_setParamValue(lua_State* L);
	static int native_getParamValue(lua_State* L);
	static int native_getLightValue(lua_State* L);
	static int native_addCable(lua_State* L);
	static int native_removeCable(lua_State* L);
	static int native_sendMidiMessage(lua_State* L);
	static int native_sendSysex(lua_State* L);
	static LuaJITEngine* getEngine(lua_State* L);
	// private
	Lune* module = NULL;
	lua_State* L = NULL;
};
