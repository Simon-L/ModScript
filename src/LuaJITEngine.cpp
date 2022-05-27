// LuaJITEngine.cpp
// Mostly adapted from the LuaJIT engine in VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/src/LuaJITEngine.cpp
// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021 Modifications by Simon-L

#include "LuaJITEngine.hpp"
#include <string>
#include <ostream>
#include <rack.hpp>

lua_State* L = NULL;

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

LuaJITEngine::~LuaJITEngine() {
	if (L)
		lua_close(L);
}

std::string LuaJITEngine::getEngineName() {
	return "Lua";
}

int LuaJITEngine::run(const std::string& path, const std::string& script) {
	ProcessBlock* block = getProcessBlock();

	// Initialize all the pointers with an offset of -1
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Warray-bounds"
	luaBlock.knobs = &block->knobs[-1];
	luaBlock.light = &block->light[-1];
	luaBlock.button = block->button;

	for (int i = 0; i < NUM_ROWS; i++) {
		luaBlock.inputs[i + 1] = &block->inputs[i][-1];
		luaBlock.outputs[i + 1] = &block->outputs[i][-1];
	}
	for (int i = 0; i < MAX_MIDI_MESSAGES; i++) {
		luaBlock.midiInput[i + 1] = &block->midiInput[i][-1];
	}
#pragma GCC diagnostic pop

	L = luaL_newstate();
	if (!L) {
		DEBUG("Could not create LuaJIT context");
		return -1;
	}

	// Import a subset of the standard library
	static const luaL_Reg lj_lib_load[] = {
		{"",              luaopen_base},
		{LUA_LOADLIBNAME, luaopen_package},
		{LUA_TABLIBNAME,  luaopen_table},
		{LUA_STRLIBNAME,  luaopen_string},
		{LUA_MATHLIBNAME, luaopen_math},
		{LUA_BITLIBNAME,  luaopen_bit},
		{LUA_JITLIBNAME,  luaopen_jit},
		{LUA_FFILIBNAME,  luaopen_ffi},
		{NULL,            NULL}
	};
	for (const luaL_Reg* lib = lj_lib_load; lib->func; lib++) {
		lua_pushcfunction(L, lib->func);
		lua_pushstring(L, lib->name);
		lua_call(L, 1, 0);
	}

	// Set user pointer
	lua_pushlightuserdata(L, this);
	lua_setglobal(L, "_engine");

	// Set global functions
	// lua_pushcfunction(L, native_print);
	// lua_setglobal(L, "print");

	lua_pushcfunction(L, native_display);
	lua_setglobal(L, "display");

	lua_pushcfunction(L, native_setParamValue);
	lua_setglobal(L, "__setParamValue");
	lua_pushcfunction(L, native_getParamValue);
	lua_setglobal(L, "__getParamValue");
	lua_pushcfunction(L, native_addCable);
	lua_setglobal(L, "__addCable");
	lua_pushcfunction(L, native_removeCable);
	lua_setglobal(L, "__removeCable");
	lua_pushcfunction(L, native_sendMidiMessage);
	lua_setglobal(L, "__sendMidiMessage");

	// Set config
	lua_newtable(L);
	{
		// frameDivider
		lua_pushinteger(L, 32);
		lua_setfield(L, -2, "frameDivider");
		// bufferSize
		lua_pushinteger(L, 1);
		lua_setfield(L, -2, "bufferSize");
	}
	lua_setglobal(L, "config");

	// Load the FFI auxiliary functions.
	std::stringstream ffi_stream;
	ffi_stream
	<< "local ffi = require('ffi')" << std::endl
	// Describe the struct `LuaProcessBlock` so that LuaJIT knows how to access the data
	<< "ffi.cdef[[" << std::endl
	<< "struct LuaProcessBlock {" << std::endl
	<< "float sampleRate;" << std::endl
	<< "float sampleTime;" << std::endl
	<< "int bufferSize;" << std::endl
	<< "int midiInputSize;" << std::endl
	<< "float *inputs[" << NUM_ROWS + 1 << "];" << std::endl
	<< "float *outputs[" << NUM_ROWS + 1 << "];" << std::endl
	<< "uint8_t *midiInput[" << MAX_MIDI_MESSAGES + 1 << "];" << std::endl
	<< "float *knobs;" << std::endl
	<< "float *light;" << std::endl
	<< "bool button;" << std::endl
	<< "};]]" << std::endl
	// Declare the function `_castBlock` used to transform `luaBlock` pointer into a LuaJIT cdata
	<< "_ffi_cast = ffi.cast" << std::endl
	<< "function _castBlock(b) return _ffi_cast('struct LuaProcessBlock*', b) end" << std::endl;
	std::string ffi_script = ffi_stream.str();

	// Compile the ffi script
	if (luaL_loadbuffer(L, ffi_script.c_str(), ffi_script.size(), "ffi_script.lua")) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	// Run the ffi script
	if (lua_pcall(L, 0, 0, 0)) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	std::string lib_dir = asset::plugin(pluginInstance, "scripts" PATH_SEPARATOR "lib");
	std::stringstream lib_stream;
	lib_stream
	<< "package.path = \"" << lib_dir << PATH_SEPARATOR "?.lua;\" .. package.path" << std::endl
	<< "local lib = require('lib')" << std::endl;
	std::string lib_script = lib_stream.str();

	// Compile the lib script
	if (luaL_loadbuffer(L, lib_script.c_str(), lib_script.size(), "lib_script.lua")) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	// Run the lib script
	if (lua_pcall(L, 0, 0, 0)) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	// Compile user script
	if (luaL_loadbuffer(L, script.c_str(), script.size(), path.c_str())) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	// Run script
	if (lua_pcall(L, 0, 0, 0)) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	// Get config
	lua_getglobal(L, "config");
	{
		// frameDivider
		lua_getfield(L, -1, "frameDivider");
		int frameDivider = lua_tointeger(L, -1);
		setFrameDivider(frameDivider);
		lua_pop(L, 1);
		// bufferSize
		lua_getfield(L, -1, "bufferSize");
		int bufferSize = lua_tointeger(L, -1);
		setBufferSize(bufferSize);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	// Get process function
	lua_getglobal(L, "process");
	if (!lua_isfunction(L, -1)) {
		DEBUG("No process() function");
		return -1;
	}

	// Create block object
	lua_getglobal(L, "_castBlock");
	lua_pushlightuserdata(L, (void*) &luaBlock);
	if (lua_pcall(L, 1, 1, 0)) {
		const char* s = lua_tostring(L, -1);
		WARN("LuaJIT: Error casting block: %s", s);
		DEBUG(s);
		lua_pop(L, 1);
		return -1;
	}

	return 0;
}

int LuaJITEngine::process() {
	ProcessBlock* block = getProcessBlock();

	// Update the values of the block.
	// The pointer values do not change.
	luaBlock.sampleRate = block->sampleRate;
	luaBlock.sampleTime = block->sampleTime;
	luaBlock.bufferSize = block->bufferSize;
	luaBlock.button = block->button;
	luaBlock.midiInputSize = block->midiInputSize;

	// Duplicate process function
	lua_pushvalue(L, -2);
	// Duplicate block
	lua_pushvalue(L, -2);
	// Call process function
	if (lua_pcall(L, 1, 0, 0)) {
		const char* err = lua_tostring(L, -1);
		WARN("LuaJIT: %s", err);
		DEBUG(err);
		return -1;
	}

	return 0;
}

LuaJITEngine* LuaJITEngine::getEngine(lua_State* L) {
	lua_getglobal(L, "_engine");
	LuaJITEngine* engine = (LuaJITEngine*) lua_touserdata(L, -1);
	lua_pop(L, 1);
	return engine;
}

// int native_print(lua_State* L) {
// 	lua_getglobal(L, "tostring");
// 	lua_pushvalue(L, 1);
// 	lua_call(L, 1, 1);
// 	const char* s = lua_tostring(L, 1);
// 	INFO("LuaJIT: %s", s);
// 	return 0;
// }

int LuaJITEngine::native_setParamValue(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 3) {
		DEBUG("LuaJIT: getParamValue: exactly 3 parameters needed, %d were provided", n);
		return 0;
	}
	int64_t moduleId = lua_tointeger(L, 1);
	int paramId = lua_tointeger(L, 2);
	lua_Number paramValue = lua_tonumber(L, 3);
	getEngine(L)->setParamValue(moduleId, paramId, paramValue, false);
	return 0;
}

int LuaJITEngine::native_getParamValue(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 2) {
		DEBUG("LuaJIT: getParamValue: exactly 2 parameters needed, %d were provided", n);
		return 0;
	}
	int64_t moduleId = lua_tointeger(L, 1);
	int paramId = lua_tointeger(L, 2);
	double paramValue = getEngine(L)->getParamValue(moduleId, paramId);
	lua_pushnumber(L, paramValue);
	return 1;
}

int LuaJITEngine::native_addCable(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 4) {
		DEBUG("LuaJIT: addCable: exactly 4 parameters needed, %d were provided", n);
		return 0;
	}
	int64_t outputModuleId = lua_tointeger(L, 1);
	int outputId = lua_tointeger(L, 2);
	int64_t inputModuleId = lua_tointeger(L, 3);
	int inputId = lua_tointeger(L, 4);
	int64_t cableId = getEngine(L)->addCable(outputModuleId, outputId, inputModuleId, inputId);
	lua_pushinteger(L, cableId);
	return 1;
}

int LuaJITEngine::native_removeCable(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 1) {
		DEBUG("LuaJIT: removeCable: exactly 1 parameters needed, %d were provided", n);
		return 0;
	}
	int64_t cableId = lua_tointeger(L, 1);
	bool ok = getEngine(L)->removeCable(cableId);
	lua_pushinteger(L, ok ? 1 : 0);
	return 1;
}

int LuaJITEngine::native_sendMidiMessage(lua_State* L) {
	int n = lua_gettop(L);
	if (n != 4) {
		DEBUG("LuaJIT: sendMidiMessage: exactly 4 parameters needed, %d were provided", n);
		return 0;
	}
	uint8_t status = lua_tointeger(L, 1);
	uint8_t channel = lua_tointeger(L, 2);
	uint8_t note = lua_tointeger(L, 3);
	uint8_t value = lua_tointeger(L, 4);
	bool ok = getEngine(L)->sendMidiMessage(status, channel, note, value);
	lua_pushinteger(L, ok ? 1 : 0);
	return 1;
}

int LuaJITEngine::native_display(lua_State* L) {
	lua_getglobal(L, "tostring");
	lua_pushvalue(L, 1);
	lua_call(L, 1, 1);
	const char* s = lua_tostring(L, -1);
	if (!s)
		s = "(null)";
	getEngine(L)->display(s);
	return 0;
}

