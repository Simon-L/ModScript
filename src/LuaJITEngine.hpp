// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021 Modifications by Simon-L

#pragma once
#include <rack.hpp>
#include "plugin.hpp"
#include <luajit-2.0/lua.hpp>

static const int NUM_ROWS = 4;
static const int MAX_BUFFER_SIZE = 4096;
static const int MAX_MIDI_MESSAGES = 1024;

struct Cardinalua;

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
	bool _switch;
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

	// Communication with Prototype module.
	// These cannot be called from your constructor, so initialize your engine in the run() method.
	void display(const std::string& message);
	void setParamValue(const int64_t moduleId, const int paramId, const double paramValue);
	double getParamValue(const int64_t moduleId, const int paramId);
	void setFrameDivider(int frameDivider);
	void setBufferSize(int bufferSize);
	ProcessBlock* getProcessBlock();
	static int native_display(lua_State* L);
	static int native_setParamValue(lua_State* L);
	static int native_getParamValue(lua_State* L);
	static LuaJITEngine* getEngine(lua_State* L);
	// private
	Cardinalua* module = NULL;
};
