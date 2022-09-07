// Lune.cpp
// Mostly adapted from VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/src/Prototype.cpp
// Copyright © 2019-2021 Andrew Belt and VCV Prototype contributors.
// 2021-2022 Modifications by Simon-L
#include "Lune.hpp"
#include "Widgets.hpp"

void Lune::processExpMessage() {
	GenericMidiExpanderMessage* _curMsg = (GenericMidiExpanderMessage*)leftExpander.consumerMessage;
	DEBUG("MIDI process: %01x %d %d %d %ld", _curMsg->msg.getStatus(), _curMsg->msg.getChannel(), _curMsg->msg.getNote(), _curMsg->msg.getValue(), _curMsg->msg.getFrame());
	if (scriptEngine) {
		assert(midiMessages.size() < midiMessages.capacity());
		midiMessages.push_back(*_curMsg);
	}
}

Lune::Lune() {
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

	cables.reserve(MAX_CABLES);
	cables.clear();

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


#ifdef USING_CARDINAL_NOT_RACK
		// cardinal specific
		midiOutputMessages.reserve(MAX_MIDI_MESSAGES);
		midiOutputMessages.clear();
		midiOutput.pcontext = static_cast<CardinalPluginContext*>(APP);
		midiInput.pcontext = static_cast<CardinalPluginContext*>(APP);
#endif
}

Lune::~Lune() {
	path = "";
	script = "";
	setScript();
	delete block;

	for (int id = 0; id < 256; id++) {
		APP->engine->removeParamHandle(&tempHandles[id]);
	}
	for (size_t i = 0; i < cables.size(); ++i) {
		Cable* cable = cables[i];
		rack::app::CableWidget* cw = APP->scene->rack->getCable(cable->id);
		APP->scene->rack->removeCable(cw);
		cw->inputPort = NULL;
		cw->outputPort = NULL;
		cw->updateCable();
	}
}

void Lune::requestRemoveAllCables() {
	for (size_t i = 0; i < cables.size(); ++i) {
		LuaCable* cable = (LuaCable*)cables[i];
		if (cable->id) {
			cable->luaId = -1;
		}
	}
	removeCableRequested = true;
}

void Lune::onReset() {
    midiInput.reset();
    midiOutput.reset();
	setScript();
}

int Lune::populateUserScripts(std::string dir){
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


#ifdef USING_CARDINAL_NOT_RACK
void Lune::processTerminalInput(const ProcessArgs& args) {
}

void Lune::process(const ProcessArgs& args) {}

void Lune::processTerminalOutput(const ProcessArgs& args)
#else
void Lune::process(const ProcessArgs& args)
#endif
{

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

		for (int id = 0; id < 256; id++) {
			if (tempHandles[id].moduleId > 0) {
				tempHandles[id].expiry -= args.sampleTime * block->bufferSize;
				if (tempHandles[id].expiry <= 0.0) {
					tempHandles[id].expiry = 0.0;
					APP->engine->updateParamHandle_NoLock(&tempHandles[id], -1, 0, true);
				}
			}
		}

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
		block->button = params[2].getValue() > 0.f;

		for (int i = 0; i < 2; i++)
			block->knobs[i] = params[i].getValue();
		float oldKnobs[2];
		std::memcpy(oldKnobs, block->knobs, sizeof(oldKnobs));

		// Run process function
		{
			// Process buffer
			if (scriptEngine != NULL) {
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

#ifdef USING_CARDINAL_NOT_RACK
	// process midi here
	for (size_t i = 0; i < midiOutputMessages.size(); i++) {
		midiOutput.sendMessage(midiOutputMessages[i]);
	}
	midiOutputMessages.clear();
#endif
}

void Lune::loadScript() {
#ifdef USING_CARDINAL_NOT_RACK
	DEBUG("path %s", scriptsDir.c_str());
	Lune *module = this;
	async_dialog_filebrowser(false, scriptsDir.c_str(), "Open Lua script", [module](char* path) {
		module->path = path;
	});
#else
	osdialog_filters* filters = osdialog_filters_parse("Lua script:lua");
	char* pathC = osdialog_file(OSDIALOG_OPEN, scriptsDir.c_str(), NULL, filters);
	if (!pathC) {
		return;
	}
	osdialog_filters_free(filters);
	this->path = pathC;
	std::free(pathC);
#endif

	DEBUG("Script path: %s", this->path.c_str());
	setScript();
}

void Lune::setScript() {
	requestRemoveAllCables();
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

int Lune::getEmptySlot() {
	for (int id = 0; id < 256; id++) {
		if (tempHandles[id].moduleId < 0) {
			return id;
		}
	}
	return -1;
}

int Lune::isExistingSlot(const int64_t moduleId, const int paramId) {
	for (int id = 0; id < 256; id++) {
		if ((tempHandles[id].moduleId == moduleId) && (tempHandles[id].paramId == paramId)) {
			return id;
		}
	}
	return -1;
}

json_t* Lune::dataToJson() {
	json_t* rootJ = json_object();

	json_object_set_new(rootJ, "autoReload", json_boolean(this->autoReload));

	json_object_set_new(rootJ, "path", json_string(path.c_str()));
	return rootJ;
}

void Lune::dataFromJson(json_t* rootJ) {
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

bool Lune::cableExists(Module* outputModule, int outputId, Module* inputModule, int inputId) {
	for (LuaCable* cable : cables) {
		if (cable->outputModule == outputModule &&
			cable->outputId == outputId &&
			cable->inputModule == inputModule &&
			cable->inputId == inputId &&
			APP->engine->hasCable((Cable*)cable)) {
			return true;
		}
	}
	return false;
}

Cable* Lune::getLuaCable(const int64_t id) {
	for (size_t i = 0; i < cables.size(); ++i)
	{
		if (cables[i]->luaId == id)
			return cables[i];
	}
	return (Cable*)NULL;
}

struct LuneWidget : ModuleWidget {
	HoveredNameLabel* lastHoveredName;
	HoveredParameterLabel* lastHoveredParameter;
	HoveredIdItem* lastHoveredId;
	int64_t hoveredModule;
	int64_t hoveredParam;
	std::string hoveredModuleName;
	std::string hoveredParameterName;

	void step() override {
		if (module) {
			Lune* _module = dynamic_cast<Lune*>(module);
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
			if (_module->addCableRequested) {
				DEBUG("Warning: Adding and removing cables is considered experimental!");
				for (size_t i = 0; i < _module->cables.size(); ++i) {
					if (_module->cables[i]->id == -1) {
						int64_t cabId = i;
						DEBUG("ADD cable at id %ld", cabId);
						DEBUG("inmod %lx in %d outmod %lx out %d", _module->cables[cabId]->inputModule->getId(), _module->cables[cabId]->inputId, _module->cables[cabId]->outputModule->getId(), _module->cables[cabId]->outputId);
						APP->engine->addCable(_module->cables[cabId]);
						rack::app::CableWidget* cw = new rack::app::CableWidget;
						cw->setCable(_module->cables[cabId]);
						cw->color = NVGcolor{0.76f, 0.11f, 0.22f, 1.00f};
				    	if (cw->isComplete()) {
			            	APP->scene->rack->addCable(cw);
				    	}
						DEBUG("luaCable %ld has id %ld", cabId, _module->cables[cabId]->id);
					}
				}
				_module->addCableRequested = false;
			}
			if (_module->removeCableRequested) {
				DEBUG("Warning: Adding and removing cables is considered experimental!");
				std::vector<Cable*> removed;
				for (size_t i = 0; i < _module->cables.size(); ++i) {
					LuaCable* cable = (LuaCable*)_module->cables[i];
					if (cable->luaId == -1) {
						DEBUG("REMOVING cable %ld at position %ld", cable->id, i);
						DEBUG("inmod %lx in %d outmod %lx out %d", cable->inputModule->getId(), cable->inputId, cable->outputModule->getId(), cable->outputId);
						rack::app::CableWidget* cw = APP->scene->rack->getCable(cable->id);
						APP->scene->rack->removeCable(cw);
						cw->inputPort = NULL;
						cw->outputPort = NULL;
						cw->updateCable();
						removed.push_back(cable);
					}
				}
				DEBUG("cables size before is %zu", _module->cables.size());
				for (Cable* cable : removed) {
					_module->cables.erase(std::remove(_module->cables.begin(), _module->cables.end(), cable), _module->cables.end());
				}
				DEBUG("cables size after is %zu", _module->cables.size());
				_module->removeCableRequested = false;
			}
		}
		ModuleWidget::step();
	}

	LuneWidget(Lune* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Lune.svg")));

		addChild(createLightCentered<SmallSimpleLight<RedGreenBlueLight>>(mm2px(Vec(10.1605, 12.0)), module, Lune::SCRIPT_LIGHT));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(5.2, 39)), module, Lune::SCRIPT_KNOB1));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 39)), module, Lune::SCRIPT_KNOB2));

		addParam(createParamCentered<PB61303>(mm2px(Vec(20.32/2, 21)), module, Lune::SCRIPT_BUTTON));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 64.342)), module, Lune::SCRIPT_INPUT1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 80.603)), module, Lune::SCRIPT_INPUT2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 96.859)), module, Lune::SCRIPT_INPUT3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 113.115)), module, Lune::SCRIPT_INPUT4));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 64.347)), module, Lune::SCRIPT_OUTPUT1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 80.603)), module, Lune::SCRIPT_OUTPUT2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 96.707)), module, Lune::SCRIPT_OUTPUT3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 113.115)), module, Lune::SCRIPT_OUTPUT4));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuEntry);

		Lune* _module = dynamic_cast<Lune*>(module);
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

Model* modelLune = createModel<Lune, LuneWidget>("Lune");