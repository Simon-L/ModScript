#include "plugin.hpp"
#include <osdialog.h>
#include <mutex>
#include <fstream>
#include <glob.h>
#include <sys/stat.h>
#include "LuaJITEngine.hpp"
#include "expanders.hpp"

struct LuaCable : Cable {
	int64_t luaId;
};

struct ExpiringParamHandle : ParamHandle {
	float expiry;

	ExpiringParamHandle() {
		color.r = 0.76f;
		color.g = 0.11f;
		color.b = 0.22f;
		color.a = 0.5f;
	}
};

struct MidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
	void onMessage(const midi::Message& message) override {
		Output::sendMessage(message);
	}

	void reset() {
		Output::reset();
		MidiGenerator::reset();
	}
};

struct Lune : ModScriptExpander, Module {

	MidiOutput midiOutput;

	void sendExpMessage(const midi::Message& msg) override {}

    void processExpMessage() override;

	enum ParamId {
		SCRIPT_KNOB1,
		SCRIPT_KNOB2,
		SCRIPT_BUTTON,
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

	bool addCableRequested = false;
	bool removeCableRequested = false;
	std::vector<LuaCable*> cables;

	Lune();

	~Lune();

	void requestRemoveAllCables();

	void onReset() override;

	int populateUserScripts(std::string dir);

	void process(const ProcessArgs& args) override;

	void loadScript();

	void setScript();

	int getEmptySlot();

	int isExistingSlot(const int64_t moduleId, const int paramId);

	json_t* dataToJson() override;

	void dataFromJson(json_t* rootJ) override;

	bool cableExists(Module* outputModule, int outputId, Module* inputModule, int inputId);

	Cable* getLuaCable(const int64_t id);

};
