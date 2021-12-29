#include "Lune.hpp"
#include "LuaJITEngine.hpp"


void LuaJITEngine::display(const std::string& message) {
	DEBUG("%s", message.c_str());
	module->message = message;
}

void LuaJITEngine::setFrameDivider(int frameDivider) {
	module->frameDivider = std::max(frameDivider, 1);
}

void LuaJITEngine::setBufferSize(int bufferSize) {
	module->block->bufferSize = clamp(bufferSize, 1, MAX_BUFFER_SIZE);
}

ProcessBlock* LuaJITEngine::getProcessBlock() {
	return module->block;
}

void LuaJITEngine::setParamValue(const int64_t moduleId, const int paramId, const double paramValue) {
	DEBUG("Module %lx param %d value %f", moduleId, paramId, (float)paramValue);
	rack::engine::Engine* eng = APP->engine;
	Module* mod = eng->getModule(moduleId);
	if (!mod) {
		DEBUG("No module with that Id: %d", mod == NULL);
		return;
	}
	if (paramId >= mod->getNumParams()) {
		DEBUG("No parameter with that Id");
		return;
	}
	eng->setParamValue(mod, paramId, paramValue);
	int id = module->isExistingSlot(moduleId, paramId);
	if (id >= 0) {
		module->tempHandles[id].expiry = module->tempHandlesExpiry;
		DEBUG("%d exists, expiry set to %f", id, module->tempHandles[id].expiry);
	} else {
		id = module->getEmptySlot();
		if (id < 0)
			return;
		eng->updateParamHandle_NoLock(&module->tempHandles[id], moduleId, paramId, true);
		module->tempHandles[id].expiry = module->tempHandlesExpiry;
		DEBUG("New handle %d. Expiry is %f", id, module->tempHandles[id].expiry);
	}
}

double LuaJITEngine::getParamValue(const int64_t moduleId, const int paramId) {
	DEBUG("Module %lx Parameter %d", moduleId, paramId);
	rack::engine::Engine* eng = APP->engine;
	Module* mod = eng->getModule(moduleId);
	if (!mod) {
		DEBUG("No module with that Id: %d", mod == NULL);
		return 0.0;
	}
	if (paramId >= mod->getNumParams()) {
		DEBUG("No parameter with that Id");
		return 0.0;
	}
	return eng->getParamValue(mod, paramId);
}

int64_t LuaJITEngine::addCable(const int64_t outputModuleId, const int outputId, const int64_t inputModuleId, const int inputId) {
	LuaCable* cable = new LuaCable;
	cable->outputModule = APP->engine->getModule(outputModuleId);
	cable->outputId = outputId;
	cable->inputModule = APP->engine->getModule(inputModuleId);
	cable->inputId = inputId;
	if (module->cableExists(cable->outputModule, cable->outputId, cable->inputModule, cable->inputId)) {
		DEBUG("Cable exists, not adding");
		return -1;
	}
	DEBUG("cables size %zu", module->cables.size());
	cable->luaId = module->cables.size();
	DEBUG("new cable id %ld", cable->luaId);
	module->cables.push_back(cable);
	DEBUG("cables size is now %zu", module->cables.size());
	module->addCableRequested = true;
	return cable->luaId;
};


bool LuaJITEngine::removeCable(const int64_t cableId) {
	rack::engine::Engine* eng = APP->engine;
	LuaCable* cable = (LuaCable*)module->getLuaCable(cableId);
	if (eng->hasCable(cable)) {
		DEBUG("REMOVE requested for cable %ld at position %ld", cable->id, cableId);
		cable->luaId = -1;
		module->removeCableRequested = true;
		return true;
	}
	return false;
}