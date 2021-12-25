// Cardinalua.cpp
// Mostly adapted from VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/src/Prototype.cpp
// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021 Modifications by Simon-L

#include "plugin.hpp"
#include <osdialog.h>
#include <mutex>
#include <fstream>
#include <glob.h>
#include <sys/stat.h>
#include "LuaJITEngine.hpp"
#include "expanders.hpp"

struct ExpiringParamHandle : ParamHandle {
	float expiry;

	ExpiringParamHandle() {
		color.r = 0.76f;
		color.g = 0.11f;
		color.b = 0.22f;
		color.a = 0.5f;
	}
};

struct Cardinalua : ModScriptExpander, Module {

	void sendExpMessage(const midi::Message& msg) override {}

    void processExpMessage() override {
		GenericMidiExpanderMessage* _curMsg = (GenericMidiExpanderMessage*)leftExpander.consumerMessage;
		DEBUG("MIDI process: %01x %d %d %d %ld", _curMsg->msg.getStatus(), _curMsg->msg.getChannel(), _curMsg->msg.getNote(), _curMsg->msg.getValue(), _curMsg->msg.getFrame());
		if (scriptEngine) {
			assert(midiMessages.size() < midiMessages.capacity());
			midiMessages.push_back(*_curMsg);
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

	std::string scriptsDir;
	std::vector<std::string> scriptFiles;

	ExpiringParamHandle tempHandles[256];
	float tempHandlesExpiry = 0.55;

	Cardinalua() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		for (int i = 0; i < 2; i++)
			configParam(i, 0.f, 1.f, 0.5f, string::f("Knob %d", i + 1));
		configParam(2, 0.f, 1.f, 0.f, string::f("Switch 1"));

		block = new ProcessBlock;
		path = "";
		script = "";
		setScript();

		midiMessages.reserve(MAX_MIDI_MESSAGES);
		midiMessages.clear();

		setupExpanding(this);

		scriptsDir = asset::plugin(pluginInstance, "scripts");
		populateUserScripts(scriptsDir);
		for (size_t i = 0; i < scriptFiles.size(); ++i)
		{
			DEBUG("Found script %s", scriptFiles[i].c_str());
		}

		for (int id = 0; id < 256; id++) {
			APP->engine->addParamHandle(&tempHandles[id]);
		}
	}

	~Cardinalua() {
		path = "";
		script = "";
		setScript();
		delete block;

		for (int id = 0; id < 256; id++) {
			APP->engine->removeParamHandle(&tempHandles[id]);
		}
	}

	void onReset() override {
		setScript();
	}

	int populateUserScripts(std::string dir){
		glob_t glob_result;
    	memset(&glob_result, 0, sizeof(glob_result));

    	std::string pattern = dir + PATH_SEPARATOR "*.lua";
    	int return_value = glob(pattern.c_str(), GLOB_TILDE, NULL, &glob_result);
    	if(return_value != 0) {
	        globfree(&glob_result);
	        DEBUG("User scripts glob() failed with return_value %d", return_value);
	        return -1;
	    }

	    for(size_t i = 0; i < glob_result.gl_pathc; ++i) {
	    	std::string path = std::string(glob_result.gl_pathv[i]);
	    	size_t sep = path.find_last_of(PATH_SEPARATOR);
	        scriptFiles.push_back(path.substr(sep + 1, path.size()));
	    }

	    globfree(&glob_result);

	    return 0;
	}

	void process(const ProcessArgs& args) override {

		bool expanderPresent = (leftExpander.module && isModScriptExpander(leftExpander.module));
        if (expanderPresent && leftExpander.messageFlipRequested) {
        	processExpRequested = true;
        }
        else if (expanderPresent && processExpRequested) {
            processExpMessage();
            processExpRequested = false;
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

		for (int id = 0; id < 256; id++) {
			if (tempHandles[id].moduleId > 0) {
				tempHandles[id].expiry -= args.sampleTime;
				if (tempHandles[id].expiry <= 0.0) {
					tempHandles[id].expiry = 0.0;
					APP->engine->updateParamHandle_NoLock(&tempHandles[id], -1, 0, true);
					DEBUG("%d has expired, removed, expiry %f", id, tempHandles[id].expiry);
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
		osdialog_filters* filters = osdialog_filters_parse("Lua script:lua");
		char* pathC = osdialog_file(OSDIALOG_OPEN, scriptsDir.c_str(), NULL, filters);
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

	int getEmptySlot() {
		for (int id = 0; id < 256; id++) {
			if (tempHandles[id].moduleId < 0) {
				return id;
			}
		}
		return -1;
	}

	int isExistingSlot(const int64_t moduleId, const int paramId) {
		for (int id = 0; id < 256; id++) {
			DEBUG("Evaluating isExistingSlot %d", id);
			if ((tempHandles[id].moduleId == moduleId) && (tempHandles[id].paramId == paramId)) {
				DEBUG("Evaluated isExistingSlot %d YES", id);
				return id;
			}
		}
		return -1;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "autoReload", json_boolean(this->autoReload));

		json_object_set_new(rootJ, "path", json_string(path.c_str()));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* reload = json_object_get(rootJ, "autoReload");
		if (reload)
			this->autoReload = json_is_true(reload);


		json_t* pathJ = json_object_get(rootJ, "path");
		if (pathJ) {
			std::string path = json_string_value(pathJ);
			this->script = "";
			this->path = path;
			setScript();
		}
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

struct UserScriptItem : MenuItem {
	Cardinalua* module;
	std::string newPath;

	UserScriptItem(Cardinalua* module, std::string name){
		this->text = name;
		this->module = module;
		this->newPath = module->scriptsDir + PATH_SEPARATOR + name;
	}

	void onAction(const event::Action& e) override {
		module->path = newPath;
		module->setScript();
	}
};

struct UserScriptsMenu : MenuItem {
	Cardinalua* module;
	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		if (module) {
			for (size_t i = 0; i < module->scriptFiles.size(); i++) {
				UserScriptItem* it = new UserScriptItem(module, module->scriptFiles[i]);
				menu->addChild(it);
			}
		}
		return menu;
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

		UserScriptsMenu* scriptsMenu = new UserScriptsMenu;
		scriptsMenu->text = "User scripts";
		scriptsMenu->rightText = RIGHT_ARROW;
		scriptsMenu->module = _module;
		menu->addChild(scriptsMenu);

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