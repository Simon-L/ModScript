#include "plugin.hpp"
#include "Lune.hpp"
#include "Widgets.hpp"

struct PleineLune : Lune {

	PleineLune() {

		for (int i = 0; i < 2; i++)
			configParam(i, 0.f, 1.f, 0.5f, string::f("Knob %d", i + 1));
		configParam(2, 0.f, 1.f, 0.f, string::f("Switch 1"));

		configLight(0, "Script RGB");

		block = new ProcessBlock;
		path = "";
		script = "";
		setScript();

		midiMessages.reserve(MAX_MIDI_MESSAGES);
		midiMessages.clear();

		cables.reserve(MAX_CABLES);
		cables.clear();

		setupExpanding(this);
	}


	void onReset() override {
		setScript();
#ifndef USING_CARDINAL_NOT_RACK
		midiInput.reset();
		midiOutput.reset();
#endif
	}

#ifdef USING_CARDINAL_NOT_RACK
	void processTerminalInput(const ProcessArgs& args) override {}

	void process(const ProcessArgs& args) {}

	void processTerminalOutput(const ProcessArgs& args) override
#else
	void process(const ProcessArgs& args) override
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

#ifdef USING_CARDINAL_NOT_RACK
		    if (midiInput.process(isBypassed(), block)) {
		        midiOutput.frame = 0;
		    } else {
		        ++midiOutput.frame;
		    }
#endif
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
#ifdef USING_CARDINAL_NOT_RACK
			// All done in ProcessTerminalInput
#else
			midi::Message msg;
			int msgIndex = 0;
			while (midiInput.tryPop(&msg, args.frame)) {
				block->midiInput[msgIndex][0] = msg.getStatus();
				block->midiInput[msgIndex][1] = msg.getChannel();
				block->midiInput[msgIndex][2] = msg.getNote();
				block->midiInput[msgIndex][3] = msg.getValue();
				msgIndex++;
			}
			block->midiInputSize = msgIndex;
#endif
			// for (size_t i = 0; i < midiMessages.size(); ++i)
			// {	
			// }
			// midiMessages.clear();

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

		return;
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();

		json_object_set_new(rootJ, "autoReload", json_boolean(this->autoReload));

		json_object_set_new(rootJ, "path", json_string(path.c_str()));
#ifndef USING_CARDINAL_NOT_RACK
		json_object_set_new(rootJ, "midiInput", midiInput.toJson());
		json_object_set_new(rootJ, "midiOutput", midiOutput.toJson());
#endif
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

#ifndef USING_CARDINAL_NOT_RACK
		json_t* midiJ = json_object_get(rootJ, "midiInput");
		if (midiJ)
			midiInput.fromJson(midiJ);
		midiJ = json_object_get(rootJ, "midiOutput");
		if (midiJ)
			midiOutput.fromJson(midiJ);
#endif
	}
};

#ifndef USING_CARDINAL_NOT_RACK
struct MidiMenu : MenuItem {
	PleineLune* module;
	// true for input, false for output
	bool select;
	midi::Port* _port;
	MidiMenu(bool select) {
		DEBUG("Yah %d", select);
		this->select = select;
	}

	Menu* createChildMenu() override {
		Menu* menu = new Menu;
		if (module) {
			if (select) {
				_port = &module->midiInput;
			}
			else {
				_port = &module->midiOutput;
			}
			DEBUG("Ha!");
			appendMidiMenu(menu, _port);
			DEBUG("Hu?!!");
		} else {
			if (module)
				DEBUG("Yes module");
			if (_port)
				DEBUG("Yes port");
			if (!module)
				DEBUG("No module");
			if (!_port)
				DEBUG("No port");
		}
		return menu;
	}
};
#endif

struct PleineLuneWidget : ModuleWidget {
	HoveredNameLabel* lastHoveredName;
	HoveredParameterLabel* lastHoveredParameter;
	HoveredIdItem* lastHoveredId;
	int64_t hoveredModule;
	int64_t hoveredParam;
	std::string hoveredModuleName;
	std::string hoveredParameterName;

	void step() override {
		if (module) {
			PleineLune* _module = dynamic_cast<PleineLune*>(module);
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
				for (Cable* cable : removed) {
					_module->cables.erase(std::remove(_module->cables.begin(), _module->cables.end(), cable), _module->cables.end());
				}
				_module->removeCableRequested = false;
			}

			if (_module->autoReload && (_module->script != "")) {
				if (_module->reloadTimer.time > 0.5) {
					_module->reloadTimer.reset();
					struct stat _st;
					stat(_module->path.c_str(), &_st);
					if (_st.st_mtime > _module->lastMtime) {
						_module->setScript();
					}
				}
			}
		}
		ModuleWidget::step();
	}

	PleineLuneWidget(PleineLune* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/PleineLune.svg")));

		addChild(createLightCentered<SmallSimpleLight<RedGreenBlueLight>>(mm2px(Vec(10.1605, 13.7+1.34)), module, PleineLune::SCRIPT_LIGHT));

		addParam(createParamCentered<PB61303>(mm2px(Vec(20.32/2, 22.5)), module, PleineLune::SCRIPT_BUTTON));

		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(5.2, 39)), module, PleineLune::SCRIPT_KNOB1));
		addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15, 39)), module, PleineLune::SCRIPT_KNOB2));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 64.342)), module, PleineLune::SCRIPT_INPUT1));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 80.603)), module, PleineLune::SCRIPT_INPUT2));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 96.859)), module, PleineLune::SCRIPT_INPUT3));
		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(5.2, 113.115)), module, PleineLune::SCRIPT_INPUT4));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 64.347)), module, PleineLune::SCRIPT_OUTPUT1));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 80.603)), module, PleineLune::SCRIPT_OUTPUT2));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 96.707)), module, PleineLune::SCRIPT_OUTPUT3));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(15, 113.115)), module, PleineLune::SCRIPT_OUTPUT4));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuEntry);

		PleineLune* _module = dynamic_cast<PleineLune*>(module);
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

#ifndef USING_CARDINAL_NOT_RACK
		menu->addChild(new MenuSeparator);
		MidiMenu* midiInputMenu = new MidiMenu(true);
		midiInputMenu->text = "MIDI input";
		midiInputMenu->rightText = RIGHT_ARROW;
		midiInputMenu->module = _module;
		menu->addChild(midiInputMenu);

		MidiMenu* midiOutputMenu = new MidiMenu(false);
		midiOutputMenu->text = "MIDI output";
		midiOutputMenu->rightText = RIGHT_ARROW;
		midiOutputMenu->module = _module;
		menu->addChild(midiOutputMenu);
#endif

		menu->addChild(new MenuSeparator);
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


Model* modelPleineLune = createModel<PleineLune, PleineLuneWidget>("PleineLune");