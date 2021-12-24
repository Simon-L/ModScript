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

	bool processExpRequested = false;

    void setupExpanding(Module *module) {
        module->leftExpander.producerMessage = new GenericMidiExpanderMessage;
        module->leftExpander.consumerMessage = new GenericMidiExpanderMessage;
    }
};