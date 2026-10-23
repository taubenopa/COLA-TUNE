#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PitchEngine.h"
#include "Scales.h"

class ColaTuneProcessor : public juce::AudioProcessor
{
public:
    ColaTuneProcessor();
    ~ColaTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "COLA-TUNE"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // --- values shared with the GUI (lock-free) ---
    std::atomic<float> uiDetectedMidi { 0.f }, uiCorrectedMidi { 0.f }, uiIn { 0.f }, uiOut { 0.f };
    std::atomic<bool>  uiVoiced { false };
    std::atomic<double> uiSampleRate { 44100.0 };
    static constexpr int scopeSize = 2048;
    std::atomic<float> scopeRing[scopeSize];
    std::atomic<int>   scopeWrite { 0 };

private:
    void updateTarget (const bool* mask, float amount, float detuneSemis);

    YinDetector detector;
    GranularShifter shifter;

    float offsetSemis = 0.f, targetOffset = 0.f, detectedMidiState = 0.f;
    float grainTarget = 512.f;
    float midiHist[3] = { 0.f, 0.f, 0.f };
    int   histCount = 0, currentNote = -1;
    bool  voiced = false;
    float wetGate = 0.f;
    juce::SmoothedValue<float> gainSmooth;

    std::atomic<float> *pRetune = nullptr, *pAmount = nullptr, *pMix = nullptr,
                       *pDetune = nullptr, *pGain = nullptr;
    std::atomic<float>* pNote[12] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColaTuneProcessor)
};
