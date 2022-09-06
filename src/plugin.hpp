#pragma once
#include <rack.hpp>

#ifdef USING_CARDINAL_NOT_RACK
#include "engine/TerminalModule.hpp"
#endif

#if defined(WIN32) || defined(_WIN32) 
#define PATH_SEPARATOR "\\" 
#else 
#define PATH_SEPARATOR "/" 
#endif 


using namespace rack;

extern std::mutex luaMutex;

// Declare the Plugin, defined in plugin.cpp
extern Plugin* pluginInstance;

// Declare each Model, defined in each module source file
extern Model* modelLune;
extern Model* modelLuneHelper;
extern Model* modelPleineLune;
