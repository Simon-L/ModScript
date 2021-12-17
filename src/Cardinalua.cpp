// Cardinalua.cpp
// Mostly adapted from VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/src/Prototype.cpp

#include "plugin.hpp"
#include <osdialog.h>
#include <mutex>
#include <fstream>

#include "LuaJITEngine.hpp"

struct Cardinalua : Module {
	enum ParamId {
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

	Cardinalua() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

		block = new ProcessBlock;
		this->message = "lolzzzqq";
		DEBUG("heeeey!");
		DEBUG("%s", this->message.c_str());
	}

	~Cardinalua() {
		delete block;
	}

	void process(const ProcessArgs& args) override {
		// Frame divider for reducing sample rate
		if (++frame < frameDivider)
			return;
		frame = 0;

		// Process block
		if (++bufferIndex >= block->bufferSize) {
			std::lock_guard<std::mutex> lock(scriptMutex);
			bufferIndex = 0;

			// Block settings
			block->sampleRate = args.sampleRate;
			block->sampleTime = args.sampleTime;

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
		}
	}

	void loadScript() {
		std::string dir = asset::plugin(pluginInstance, "examples");
		char* pathC = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		if (!pathC) {
			return;
		}
		std::string path = pathC;
		DEBUG("Script path %s", pathC);
		setScript(path);
		std::free(pathC);
	}

	void setScript(std::string path) {
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
			DEBUG("Oups!");
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

struct LoadScriptItem : MenuItem {
	Cardinalua *module;
	void onAction(const event::Action& e) override {
		module->loadScript();
	}
};

struct CardinaluaWidget : ModuleWidget {

	void step() override {
	}

	CardinaluaWidget(Cardinalua* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/Cardinalua.svg")));
	}

	void appendContextMenu(Menu* menu) override {
		menu->addChild(new MenuEntry);

		Cardinalua* _module = dynamic_cast<Cardinalua*>(module);
		assert(_module);

		LoadScriptItem* loadscript = createMenuItem<LoadScriptItem>("Open script");
		loadscript->module = _module;
		menu->addChild(loadscript);
	}
};

Model* modelCardinalua = createModel<Cardinalua, CardinaluaWidget>("Cardinalua");