// Cardinalua.cpp
// Mostly adapted from VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/src/Prototype.cpp
// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021 Modifications by Simon-L

#include "plugin.hpp"
#include <osdialog.h>
#include <mutex>
#include <fstream>
#include <sys/stat.h>
#include "LuaJITEngine.hpp"
#include "expanders.hpp"

struct Cardinalua : ModScriptExpander, Module {

	void sendExpMessage(const midi::Message& msg) override {}

	GenericMidiExpanderMessage _curMsg;
    void processExpMessage() override {
   		if (leftExpander.messageFlipRequested) {
        	_curMsg = *((GenericMidiExpanderMessage*)leftExpander.consumerMessage);
   			// DEBUG("MIDI: %01x %d %d %d", _curMsg.msg.getStatus(), _curMsg.msg.getChannel(), _curMsg.msg.getNote(), _curMsg.msg.getValue());
   			if (scriptEngine) {
   				assert(midiMessages.size() < midiMessages.capacity());
   				midiMessages.push_back(_curMsg);
   			}
   		}
    }

	enum ParamId {
		SCRIPT_KNOB1,
		SCRIPT_KNOB2,
		SCRIPT_SWITCH,
		PARAMS_LEN
	};
	enum InputId {
		SCRIPT_INPUT1,
		SCRIPT_INPUT2,
		SCRIPT_INPUT3,
		SCRIPT_INPUT4,
		INPUTS_LEN
	};
	enum OutputId {
		SCRIPT_OUTPUT1,
		SCRIPT_OUTPUT2,
		SCRIPT_OUTPUT3,
		SCRIPT_OUTPUT4,
		OUTPUTS_LEN
	};
	enum LightId {
		ENUMS(SCRIPT_LIGHT, 3),
		LIGHTS_LEN
	};

	std::string message;
	std::string path;
	std::string script;
	std::string engineName;
	std::mutex scriptMutex;
	LuaJITEngine* scriptEngine = NULL;
	int frame = 0;
	int frameDivider;
	// This is dynamically allocated to have some protection against script bugs.
	ProcessBlock* block;
	int bufferIndex = 0;

	std::vector<GenericMidiExpanderMessage> midiMessages;

	bool autoReload = false;
	dsp::Timer reloadTimer;
	time_t lastMtime = 0;

	Cardinalua() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < 2; i++)
			configParam(i, 0.f, 1.f, 0.5f, string::f("Knob %d", i + 1));
		configParam(2, 0.f, 1.f, 0.f, string::f("Switch 1"));

		block = new ProcessBlock;

		midiMessages.reserve(MAX_MIDI_MESSAGES);
		midiMessages.clear();

		setupExpanding(this);
	}

	~Cardinalua() {
		delete block;
	}

	void process(const ProcessArgs& args) override {

		bool expanderPresent = (leftExpander.module && isModScriptExpander(leftExpander.module));
        if (expanderPresent) {
            processExpMessage();
        }

		if (autoReload && (script != "")) {
			reloadTimer.process(args.sampleTime);
			if (reloadTimer.time > 0.5) {
				reloadTimer.reset();
				struct stat _st;
				stat(path.c_str(), &_st);
				if (_st.st_mtime > lastMtime) {
					setScript();
				}
			}
		}

		// Frame divider for reducing sample rate
		if (++frame < frameDivider)
			return;
		frame = 0;

		// Clear outputs if no script is running
		if (!scriptEngine) {
			for (int c = 0; c < 3; c++)
				lights[c].setBrightness(0.f);
			for (int i = 0; i < NUM_ROWS; i++)
				outputs[i].setVoltage(0.f);
			return;
		}

		// Inputs
		for (int i = 0; i < NUM_ROWS; i++)
			block->inputs[i][bufferIndex] = inputs[i].getVoltage();

		// Process block
		if (++bufferIndex >= block->bufferSize) {
			std::lock_guard<std::mutex> lock(scriptMutex);
			bufferIndex = 0;

			// Block settings
			block->sampleRate = args.sampleRate;
			block->sampleTime = args.sampleTime;

			// Midi messages
			block->midiInputSize = midiMessages.size();
			for (size_t i = 0; i < midiMessages.size(); ++i)
			{	
				block->midiInput[i][0] = midiMessages[i].msg.getStatus();
				block->midiInput[i][1] = midiMessages[i].msg.getChannel();
				block->midiInput[i][2] = midiMessages[i].msg.getNote();
				block->midiInput[i][3] = midiMessages[i].msg.getValue();
			}
			midiMessages.clear();

			// Params
			block->_switch = params[2].getValue() > 0.f;

			for (int i = 0; i < 2; i++)
				block->knobs[i] = params[i].getValue();
			float oldKnobs[2];
			std::memcpy(oldKnobs, block->knobs, sizeof(oldKnobs));

			// Run process function
			{
				// Process buffer
				if (scriptEngine) {
					if (scriptEngine->process()) {
						WARN("Script %s process() failed. Stopped script.", path.c_str());
						delete scriptEngine;
						scriptEngine = NULL;
						return;
					}
				}
			}

			// Params
			// Only set params if values were changed by the script. This avoids issues when the user is manipulating them from the UI thread.
			for (int i = 0; i < 2; i++) {
				if (block->knobs[i] != oldKnobs[i])
					params[i].setValue(block->knobs[i]);
			}
			// Lights
			for (int c = 0; c < 3; c++)
				lights[c].setBrightness(block->light[c]);
		}

		// Outputs
		for (int i = 0; i < NUM_ROWS; i++)
			outputs[i].setVoltage(block->outputs[i][bufferIndex]);

	}

	void loadScript() {
		std::string dir = asset::plugin(pluginInstance, "scripts");
		osdialog_filters* filters = osdialog_filters_parse("Lua script:lua");
		char* pathC = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, filters);
		if (!pathC) {
			return;
		}
		osdialog_filters_free(filters);

		this->path = pathC;
		DEBUG("Script path %s", pathC);
		std::free(pathC);

		setScript();
	}

	void setScript() {
		DEBUG("Loading %s", path.c_str());
		std::lock_guard<std::mutex> lock(scriptMutex);
		// Read file
		std::string script;
		std::ifstream file;
		file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
		try {
			file.open(path);
			std::stringstream buffer;
			buffer << file.rdbuf();
			script = buffer.str();
		}
		catch (const std::runtime_error& err) {
			WARN("Failed opening %s", path.c_str());
		}
		struct stat _st;
		stat(path.c_str(), &_st);
		lastMtime = _st.st_mtime;

		DEBUG("Content:\n%s", script.c_str());

		// Reset script state
		if (scriptEngine) {
			delete scriptEngine;
			scriptEngine = NULL;
		}
		this->script = "";
		this->engineName = "";
		this->message = "";
		// Reset process state
		frameDivider = 32;
		frame = 0;
		bufferIndex = 0;
		// Reset block
		*block = ProcessBlock();

		if (script == "")
			return;
		this->script = script;

		scriptEngine = new LuaJITEngine;
		if (!scriptEngine) {
			return;
		}
		scriptEngine->module = this;

		// Run script
		if (scriptEngine->run(path, script)) {
			// Error message should have been set by ScriptEngine
			delete scriptEngine;
			scriptEngine = NULL;
			return;
		}
		this->engineName = scriptEngine->getEngineName();
	}
};

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
}
double LuaJITEngine::getParamValue(const int64_t moduleId, const int paramId) {
	DEBUG("Module %lx param %d", moduleId, paramId);
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

struct LoadScriptItem : MenuItem {
	Cardinalua *module;
	void onAction(const event::Action& e) override {
		module->loadScript();
	}
};

struct AutoReloadItem : MenuItem {
	Cardinalua *module;
	void onAction(const event::Action& e) override {
		module->autoReload ^= true;
		module->reloadTimer.reset();
	}
	void step() override {
		rightText = CHECKMARK(module->autoReload);
	}
};

struct HoveredNameLabel : MenuLabel {
	std::string* name;
	void step() override {
		text = *name;
	}
};

struct HoveredParameterLabel : MenuLabel {
	std::string* name;
	int64_t* id;
	void step() override {
		if (*id < 0)
			text = string::f("Parameter: %s Id: -/-", name->c_str());
		else
			text = string::f("Parameter: %s Id: %ld", name->c_str(), *id);
	}
};

struct HoveredIdItem : MenuItem {
	int64_t* id;
	void onAction(const event::Action& e) override {
		glfwSetClipboardString(APP->window->win, string::f("0x%lx", *id).c_str());
	}
	void step() override {
		text = string::f("Id: 0x%lx", *id);
	}
};

struct CardinaluaWidget : ModuleWidget {
	HoveredNameLabel* lastHoveredName;
	HoveredParameterLabel* lastHoveredParameter;
	HoveredIdItem* lastHoveredId;
	int64_t hoveredModule;
	int64_t hoveredParam;
	std::string hoveredModuleName;
	std::string hoveredParameterName;

	void step() override {
		if (module) {
			Cardinalua* _module = dynamic_cast<Cardinalua*>(module);
			rack::widget::EventState* evState = APP->event;
			if (ModuleWidget *mwidget = dynamic_cast<ModuleWidget *>(evState->hoveredWidget)) {
				int64_t _hovId = mwidget->getModule()->getId();
				if (_hovId != hoveredModule && _hovId != _module->getId()) {
					hoveredModule = _hovId;
					hoveredParam = -1;
					hoveredParameterName = "-/-";
					// DEBUG("%lx - %s", (int64_t)hoveredModule, mwidget->getModel()->getFullName().c_str());
					hoveredModuleName = mwidget->getModel()->getFullName();
				}
			}
			if (ParamWidget *pwidget = dynamic_cast<ParamWidget *>(evState->hoveredWidget)) {
				int64_t _hovId = pwidget->paramId;
				if (_hovId != hoveredParam && pwidget->module->getId() != _module->getId()) {
					hoveredParam = _hovId;
					hoveredParameterName = pwidget->module->getParamQuantity(hoveredParam)->name;
				}
			}
		}
		ModuleWidget::step();
	}

	CardinaluaWidget(Cardinalua* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Cardinalua.svg")));

		addChild(createLightCentered<SmallSimpleLight<RedGreenBlueLight>>(mm2px(Vec(10.1605, 6.2435)), module, Cardinalua::SCRIPT_LIGHT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.32/2, 37)), module, Cardinalua::SCRIPT_KNOB1));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(20.32/2, 50)), module, Cardinalua::SCRIPT_KNOB2));

		addParam(createParamCentered<PB61303>(mm2px(Vec(20.32/2, 24)), module, Cardinalua::SCRIPT_SWITCH));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 64.342)), module, Cardinalua::SCRIPT_INPUT1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 80.603)), module, Cardinalua::SCRIPT_INPUT2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 96.859)), module, Cardinalua::SCRIPT_INPUT3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 113.115)), module, Cardinalua::SCRIPT_INPUT4));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 64.347)), module, Cardinalua::SCRIPT_OUTPUT1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 80.603)), module, Cardinalua::SCRIPT_OUTPUT2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 96.707)), module, Cardinalua::SCRIPT_OUTPUT3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 113.115)), module, Cardinalua::SCRIPT_OUTPUT4));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuEntry);

		Cardinalua* _module = dynamic_cast<Cardinalua*>(module);
		assert(_module);

		LoadScriptItem* loadscript = createMenuItem<LoadScriptItem>("Open script");
		loadscript->module = _module;
		menu->addChild(loadscript);

		AutoReloadItem* autoreload = createMenuItem<AutoReloadItem>("Auto-reload script on change");
		autoreload->module = _module;
		menu->addChild(autoreload);

		menu->addChild(createMenuLabel("Last hovered module:"));
		lastHoveredName = createMenuLabel<HoveredNameLabel>("-/-");
		lastHoveredName->name = &hoveredModuleName;
		menu->addChild(lastHoveredName);

		lastHoveredId = createMenuItem<HoveredIdItem>("Id: -/-", "Click to copy");
		lastHoveredId->id = &hoveredModule;
		menu->addChild(lastHoveredId);

		lastHoveredParameter = createMenuLabel<HoveredParameterLabel>("Parameter: -/- Id: -/-");
		lastHoveredParameter->name = &hoveredParameterName;
		lastHoveredParameter->id = &hoveredParam;
		menu->addChild(lastHoveredParameter);
	}
};

Model* modelCardinalua = createModel<Cardinalua, CardinaluaWidget>("Cardinalua");