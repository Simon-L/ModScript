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

void LuaJITEngine::setParamValue(const int64_t moduleId, const int paramId, const double paramValue, const bool normalized, const bool noIndicator, const bool relative) {
	// DEBUG("Module %lx param %d value %f", moduleId, paramId, (float)paramValue);
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
	rack::engine::ParamQuantity* quantity = mod->getParamQuantity(paramId);
	float min = quantity->getMinValue();
	float max = quantity->getMaxValue();
	if (normalized) {
		if (relative) {
			float newValue = eng->getParamValue(mod, paramId) + abs(max - min) * paramValue;
			eng->setParamValue(mod, paramId, rack::math::clamp(newValue, min, max));
		} else {
			float normValue = rack::math::rescale(paramValue, 0.0f, 1.0f, min, max);
			normValue = rack::math::clamp(normValue, min, max);
			eng->setParamValue(mod, paramId, normValue);
		}
	} else {
		if (relative) {
			eng->setParamValue(mod, paramId, rack::math::clamp(eng->getParamValue(mod, paramId) + paramValue, min, max));
		} else {
			eng->setParamValue(mod, paramId, rack::math::clamp(paramValue, min, max));
		}
	}
	if (noIndicator) {
	} else {
		int id = module->isExistingSlot(moduleId, paramId);
		if (id >= 0) {
			module->tempHandles[id].expiry = module->tempHandlesExpiry;
		} else {
			id = module->getEmptySlot();
			if (id < 0)
				return;
			eng->updateParamHandle_NoLock(&module->tempHandles[id], moduleId, paramId, true);
			module->tempHandles[id].expiry = module->tempHandlesExpiry;
		}
	}
}

double LuaJITEngine::getParamValue(const int64_t moduleId, const int paramId) {
	// DEBUG("Module %lx Parameter %d", moduleId, paramId);
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

bool LuaJITEngine::sendMidiMessage(const uint8_t status, const uint8_t channel, const uint8_t note, const uint8_t value) {
	midi::Message msg;
	msg.setStatus(status);
	msg.setChannel(channel);
	msg.setNote(note);
	msg.setValue(value);
#ifdef USING_CARDINAL_NOT_RACK
	// send midi
	module->midiOutputMessages.push_back(msg);
	return true;
#else
	DEBUG("MIDI: %ld %s", msg.getFrame(), msg.toString().c_str());
	if (module->midiOutput.getDevice() == NULL) {
		return false;
	} else {
		module->midiOutput.sendMessage(msg);
		return true;
	}
#endif
}