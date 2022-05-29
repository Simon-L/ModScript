// Hacked from plugins/Cardinal/plugincontext.hpp

static constexpr const uint32_t kModuleParameters = 24;

enum CardinalVariant {
    kCardinalVariantMain,
    kCardinalVariantFX,
    kCardinalVariantSynth,
};

class UI;

struct MidiEvent {
    static const uint32_t kDataSize = 4;
    uint32_t frame;
    uint32_t size;
    uint8_t data[kDataSize];
    const uint8_t* dataExt;
};

struct CardinalPluginContext : rack::Context {
    uint32_t bufferSize, processCounter;
    double sampleRate;
    float parameters[kModuleParameters];
    CardinalVariant variant;
    bool bypassed, playing, reset, bbtValid;
    int32_t bar, beat, beatsPerBar, beatType;
    uint64_t frame;
    double barStartTick, beatsPerMinute;
    double tick, tickClock, ticksPerBeat, ticksPerClock, ticksPerFrame;
    uintptr_t nativeWindowId;
    const float* const* dataIns;
    float** dataOuts;
    const MidiEvent* midiEvents;
    uint32_t midiEventCount;
    void* const plugin;
#ifndef HEADLESS
    UI* ui;
#endif
    CardinalPluginContext(void* const p);
    void writeMidiMessage(const rack::midi::Message& message, uint8_t channel);
#ifndef HEADLESS
    bool addIdleCallback(void* cb) const;
    void removeIdleCallback(void* cb) const;
#endif
};