#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "Parameters.h"
#include "dsp/TalkBoxEngine.h"

class BasementalTalkBoxProcessor : public juce::AudioProcessor
{
public:
    BasementalTalkBoxProcessor();
    ~BasementalTalkBoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;   // on-screen keyboard

    // meters for the editor
    float getVoiceLevel() const  { return voiceLevel.load(); }
    float getGate() const        { return gate.load(); }
    float getOutputLevel() const { return outLevel.load(); }
    bool  hasVoiceInput() const  { return voiceConnected.load(); }
    int   getActiveVoiceCount() const { return activeVoices.load(); }

private:
    void updateEngineParams();
    void handleMidi (const juce::MidiMessage& m);

    btb::TalkBoxEngine engine;
    std::vector<float> voiceBuf, outL, outR;
    int currentProgram = 0;
    bool sustainDown = false;
    std::array<bool, 128> heldBySustain {};

    std::atomic<float> voiceLevel { 0 }, gate { 0 }, outLevel { 0 };
    std::atomic<bool> voiceConnected { false };
    std::atomic<int> activeVoices { 0 };

    std::map<juce::String, std::atomic<float>*> raw;
    float P (const char* id) const { return raw.at (id)->load(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementalTalkBoxProcessor)
};
