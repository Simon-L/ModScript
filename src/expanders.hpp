struct GenericMidiExpanderMessage
{
	midi::Message msg;
};

struct ModScriptExpander
{
	ModScriptExpander(){
	};
	virtual ~ModScriptExpander(){};

	bool isModScriptExpandable(Module *x) { return x->model == modelCardinalua; };
	bool isModScriptExpander(Module *x) { return x->model == modelMIDIToExp; };

	virtual void sendExpMessage(const midi::Message& msg) = 0;
	virtual void processExpMessage() = 0;

	GenericMidiExpanderMessage rightMessages[2][1] = {}; // messages from right-side
    GenericMidiExpanderMessage leftMessages[2][1] = {};// messages from left-side

    void setupExpanding(Module *module) {
        module->leftExpander.producerMessage = leftMessages[0];
        module->leftExpander.consumerMessage = leftMessages[1];
    }
};