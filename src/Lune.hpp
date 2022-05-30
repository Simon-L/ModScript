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

#ifdef USING_CARDINAL_NOT_RACK
#include "plugincontext.hpp"

struct MidiInput {
    // Cardinal specific
    CardinalPluginContext* pcontext;
    const MidiEvent* midiEvents;
    uint32_t midiEventsLeft;
    uint32_t midiEventFrame;
    uint32_t lastProcessCounter;
    uint8_t channel;

    MidiInput() {
        reset();
    }

    void reset() {
        midiEvents = nullptr;
        midiEventsLeft = 0;
        midiEventFrame = 0;
        lastProcessCounter = 0;
        channel = 0;
    }

    bool process(const bool isBypassed, ProcessBlock* block) {
        // Cardinal specific
        const uint32_t processCounter = pcontext->processCounter;
        const bool processCounterChanged = lastProcessCounter != processCounter;

        if (processCounterChanged)
        {
            lastProcessCounter = processCounter;
            midiEvents = pcontext->midiEvents;
            midiEventsLeft = pcontext->midiEventCount;
            midiEventFrame = 0;
        }

        if (isBypassed)
        {
            ++midiEventFrame;
            return false;
        }

        block->midiInputSize = midiEventsLeft;
        uint32_t ev = 0;
        while (midiEventsLeft != 0)
        {
            const MidiEvent& midiEvent(*midiEvents);

            if (midiEvent.frame > midiEventFrame)
                break;

            ++midiEvents;
            --midiEventsLeft;

            const uint8_t* const data = midiEvent.size > MidiEvent::kDataSize
                                      ? midiEvent.dataExt
                                      : midiEvent.data;

            if (channel != 0 && data[0] < 0xF0)
            {
                if ((data[0] & 0x0F) != (channel - 1))
                    continue;
            }

            const uint8_t status = data[0] & 0xF0;
            const uint8_t chan = data[0] & 0x0F;

            block->midiInput[ev][0] = status;
			block->midiInput[ev][1] = chan;
			block->midiInput[ev][2] = data[1];
			block->midiInput[ev][3] = midiEvent.size > 2 ? data[2] : 0;
			ev++;
        }

        ++midiEventFrame;

		return processCounterChanged;
	}

};

struct MidiOutput {
    // cardinal specific
    CardinalPluginContext* pcontext;
    uint8_t channel = 0;
    int64_t frame = 0;

    void sendMessage(midi::Message& message)
    {	
    	message.setFrame(frame);
        pcontext->writeMidiMessage(message, channel);
    }

	void reset() {
	}
};
#else

struct MidiOutput : dsp::MidiGenerator<PORT_MAX_CHANNELS>, midi::Output {
	void onMessage(const midi::Message& message) override {
		Output::sendMessage(message);
	}

	void reset() {
		Output::reset();
		MidiGenerator::reset();
	}
};
#endif

#ifdef USING_CARDINAL_NOT_RACK
struct Lune : ModScriptExpander, TerminalModule {
	void processTerminalInput(const ProcessArgs& args) override ;
	void processTerminalOutput(const ProcessArgs& args) override ;
	std::vector<midi::Message> midiOutputMessages;
	MidiInput midiInput;
#else
struct Lune : ModScriptExpander, Module {
	midi::InputQueue midiInput;
#endif

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
