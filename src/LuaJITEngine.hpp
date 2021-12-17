#pragma once
#include <rack.hpp>
#include <luajit-2.0/lua.hpp>

static const int NUM_ROWS = 0;
static const int MAX_BUFFER_SIZE = 4096;

struct Cardinalua;

struct ProcessBlock {
	float sampleRate = 0.f;
	float sampleTime = 0.f;
	int bufferSize = 1;
	float inputs[NUM_ROWS][MAX_BUFFER_SIZE] = {};
	float outputs[NUM_ROWS][MAX_BUFFER_SIZE] = {};
	float knobs[NUM_ROWS] = {};
	bool switches[NUM_ROWS] = {};
	float lights[NUM_ROWS][3] = {};
	float switchLights[NUM_ROWS][3] = {};
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
	void setFrameDivider(int frameDivider);
	void setBufferSize(int bufferSize);
	ProcessBlock* getProcessBlock();
	static int native_display(lua_State* L);
	static LuaJITEngine* getEngine(lua_State* L);
	// private
	Cardinalua* module = NULL;
};
