#include "plugin.hpp"


struct LuneHelper : Module {
	enum ParamId {
		K1_PARAM,
		K2_PARAM,
		DUMP_PARAM,
		ID_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		INPUTS_LEN
	};
	enum OutputId {
		OUTPUTS_LEN
	};
	enum LightId {
		LIGHTS_LEN
	};

	dsp::BooleanTrigger dumpTrigger;
	dsp::BooleanTrigger idTrigger;
	int64_t hoveredModule;


	LuneHelper() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		// configParam(K1_PARAM, 0.f, 1.f, 0.f, "K1_PARAM");
		// configParam(K2_PARAM, 0.f, 1.f, 0.f, "K2_PARAM");
		configParam(ID_PARAM, 0.f, 1.f, 0.f, "Copy ID to clipboard");
		configParam(DUMP_PARAM, 0.f, 1.f, 0.f, "Copy module dump to clipboard");
	}

	void process(const ProcessArgs& args) override {
		if (dumpTrigger.process(params[DUMP_PARAM].getValue() > 0.f)) {
			Module* mod = APP->engine->getModule(hoveredModule);
			if (!mod) {
				return;
			}
			std::string fullname = mod->getModel()->getFullName();
			std::string pluginName = mod->getModel()->plugin->name;
			std::string modelName = mod->getModel()->name;
			std::string slug = mod->getModel()->slug;
			std::string dump = string::f("function New%s(id)\n\tmod = Module(id)\n\tmod.params = {\n", slug.c_str());
			int numParams = mod->getNumParams();
			for (int i = 0; i < numParams; ++i)
			{
				ParamQuantity* quant = mod->getParamQuantity(i);
				std::string label = quant->getLabel();
				float min = quant->minValue;
				float max = quant->maxValue;
				float _default = quant->defaultValue;
				dump += string::f("\t\t[\"%s\"] = {index = %d, min = %f, max = %f, default = %f},\n", label.c_str(), i, min, max, _default);
			}
			dump += "\t}\n\tmod.inputs = {\n";
			int numInputs = mod->getNumInputs();
			for (int i = 0; i < numInputs; ++i)
			{
				PortInfo* port = mod->getInputInfo(i);
				std::string portName = port->name;
				dump += string::f("\t\t[\"%s\"] = {index = %d},\n", portName.c_str(), i);
			}
			dump += "\t}\n\tmod.outputs = {\n";
			int numOutputs = mod->getNumOutputs();
			for (int i = 0; i < numOutputs; ++i)
			{
				PortInfo* port = mod->getOutputInfo(i);
				std::string portName = port->name;
				dump += string::f("\t\t[\"%s\"] = {index = %d},\n", portName.c_str(), i);
			}
			dump += "\t}\n\tmod.lights = {\n";
			size_t numLights = mod->lightInfos.size();
			for (size_t i = 0; i < numLights; ++i) {
				if (mod->lightInfos[i]) {
					std::string lightName = mod->lightInfos[i]->getName();
					dump += string::f("\t\t[\"%s\"] = {index = %ld},\n", lightName.c_str(), i);
				} else {
					dump += string::f("\t\t{index = %ld},\n", i);
				}
			}
			dump += "\t}\n\treturn mod\nend\n";
			glfwSetClipboardString(APP->window->win, dump.c_str());
		}

		if (idTrigger.process(params[ID_PARAM].getValue() > 0.f)) {
			// Module ID
			char hexID[32];
			sprintf(hexID, "0x%llx", hoveredModule);
			glfwSetClipboardString(APP->window->win, hexID);
		}

	}
};

struct CenteredLabel : Widget {
	std::string text;
	int fontSize;
	CenteredLabel(int _fontSize = 13) {
		fontSize = _fontSize;
		box.size.y = BND_WIDGET_HEIGHT;
	}
	void draw(const DrawArgs &args) override {
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER);
		nvgFillColor(args.vg, nvgRGB(159, 193, 217));
		nvgFontSize(args.vg, fontSize);
		nvgText(args.vg, box.pos.x, box.pos.y, text.c_str(), NULL);
	}
};

struct LuneHelperWidget : ModuleWidget {
	int64_t hoveredModule;
	int64_t hoveredParam;
	float lastValue;

	CenteredLabel* pluginLabel = NULL;
	CenteredLabel* moduleLabel = NULL;
	CenteredLabel* moduleIdLabel = NULL;
	CenteredLabel* paramLabel = NULL;
	CenteredLabel* valueLabel = NULL;
	CenteredLabel* limitsLabel = NULL;
	CenteredLabel* lightLabel = NULL;
	CenteredLabel* lightValueLabel = NULL;
	bool refresh = false;
	bool noRefresh = false;

	void step() override {
		rack::widget::EventState* evState = APP->event;
		if (ModuleWidget *mwidget = dynamic_cast<ModuleWidget *>(evState->hoveredWidget)) {
			int64_t _hovId = mwidget->getModule()->getId();
			if (_hovId == getModule()->id) {
				noRefresh = true;
				hoveredParam = -1;
				hoveredModule = -1;
			}
			else if (_hovId != hoveredModule) {
				noRefresh = false;
				hoveredModule = _hovId;
				if (module) {
					LuneHelper *_mod = dynamic_cast<LuneHelper *>(getModule());
					_mod->hoveredModule = hoveredModule;
				}
				hoveredParam = -1;
				// DEBUG("%llx - %s", (int64_t)hoveredModule, mwidget->getModel()->getFullName().c_str());
				pluginLabel->text = mwidget->getModel()->plugin->name.c_str();
				moduleLabel->text = mwidget->getModel()->name.c_str();
				moduleIdLabel->text = string::f("0x%llx", (int64_t)hoveredModule);
				paramLabel->text = "";
				valueLabel->text = "";
				limitsLabel->text = "";
			}
		}
		if (noRefresh) {
		} else if (ParamWidget *pwidget = dynamic_cast<ParamWidget *>(evState->hoveredWidget)) {
			int64_t _hovId = pwidget->paramId;
			if (_hovId != hoveredParam) {
				hoveredParam = _hovId;
				refresh = true;
			}
			auto quant = pwidget->getParamQuantity();
			if (quant->getValue() != lastValue or refresh) {
				paramLabel->text = string::f("%s (%2ld)", quant->getLabel().c_str(), (int64_t)hoveredParam);
				valueLabel->text = string::f("%s%s (%2.2f)", quant->getDisplayValueString().c_str(), quant->getUnit().c_str(), quant->getValue());
				limitsLabel->text = string::f("Min: %2.2f Max: %2.2f", quant->getMinValue(), quant->getMaxValue());
				lastValue = quant->getValue();
				refresh = false;
			}
		}
		ModuleWidget::step();
	}

	LuneHelperWidget(LuneHelper* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/LuneHelper_vector.svg")));

		addParam(createParamCentered<LEDButton>(mm2px(Vec(15.444, 80.603)), module, LuneHelper::ID_PARAM));
		addParam(createParamCentered<LEDButton>(mm2px(Vec(30.282, 80.603)), module, LuneHelper::DUMP_PARAM));

		int baseModY = 32;
		int baseParamY = 66;
		// int baseLightY = 72;
		int lineHeight = 7;
		pluginLabel = new CenteredLabel;
		pluginLabel->box.pos = Vec(35, baseModY);
		pluginLabel->text = "";
		addChild(pluginLabel);
		moduleLabel = new CenteredLabel;
		moduleLabel->box.pos = Vec(35, baseModY+lineHeight);
		moduleLabel->text = "";
		addChild(moduleLabel);
		moduleIdLabel = new CenteredLabel;
		moduleIdLabel->box.pos = Vec(35, baseModY+lineHeight*2);
		moduleIdLabel->text = "";
		addChild(moduleIdLabel);
		paramLabel = new CenteredLabel;
		paramLabel->box.pos = Vec(35, baseParamY);
		paramLabel->text = "";
		addChild(paramLabel);
		valueLabel = new CenteredLabel;
		valueLabel->box.pos = Vec(35, baseParamY+lineHeight);
		valueLabel->text = "";
		addChild(valueLabel);
		limitsLabel = new CenteredLabel;
		limitsLabel->box.pos = Vec(35, baseParamY+lineHeight*2);
		limitsLabel->text = "";
		addChild(limitsLabel);
		// addParam(createParamCentered<Trimpot>(mm2px(Vec(39.118, 80.603)), module, LuneHelper::PWMCV_PARAM));

		// addInput(createInputCentered<PJ301MPort>(mm2px(Vec(6.607, 96.859)), module, LuneHelper::FM_INPUT));
		// addInput(createInputCentered<PJ301MPort>(mm2px(Vec(17.444, 96.859)), module, LuneHelper::PITCH_INPUT));
		// addInput(createInputCentered<PJ301MPort>(mm2px(Vec(28.282, 96.859)), module, LuneHelper::SYNC_INPUT));
		// addInput(createInputCentered<PJ301MPort>(mm2px(Vec(39.15, 96.859)), module, LuneHelper::PWM_INPUT));

		// addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(6.607, 113.115)), module, LuneHelper::SIN_OUTPUT));
		// addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(17.444, 113.115)), module, LuneHelper::TRI_OUTPUT));
		// addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(28.282, 113.115)), module, LuneHelper::SAW_OUTPUT));
		// addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(39.119, 113.115)), module, LuneHelper::SQR_OUTPUT));

		// addChild(createLightCentered<SmallLight<RedGreenBlueLight>>(mm2px(Vec(31.089, 16.428)), module, LuneHelper::FREQ_LIGHT));
	}
};


Model* modelLuneHelper = createModel<LuneHelper, LuneHelperWidget>("LuneHelper");