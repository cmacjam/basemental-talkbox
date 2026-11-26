#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "BasementalLookAndFeel.h"
#include "TalkBoxStage.h"
#include "MicSetupPanel.h"

class BasementalTalkBoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BasementalTalkBoxEditor (BasementalTalkBoxProcessor&);
    ~BasementalTalkBoxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1180, kBaseH = 820;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Switch
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Section { juce::String title; juce::Rectangle<int> area; };

    class Content : public juce::Component
    {
    public:
        explicit Content (BasementalTalkBoxEditor& e) : owner (e) {}
        void paint (juce::Graphics& g) override { owner.paintContent (g); }
    private:
        BasementalTalkBoxEditor& owner;
    };

    void paintContent (juce::Graphics&);
    void layoutContent();
    void timerCallback() override;
    Knob& addKnob (const char* paramId, const juce::String& name);
    Switch& addSwitch (const char* paramId, const juce::String& name);
    void setChoice (const char* paramId, int index);

    BasementalTalkBoxProcessor& proc;
    BasementalLookAndFeel lnf;
    Content content { *this };

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Switch>> switches;
    std::map<juce::String, Knob*> knobById;
    std::map<juce::String, Switch*> switchById;

    juce::TextButton miniButton { "MINI" }, dxButton { "DX" };
    juce::TextButton voiceButton { "VOICE" }, vowelButton { "VOWEL" };
    juce::TextButton micButton;
    TalkBoxStage stage;
    std::unique_ptr<MicSetupPanel> micPanel;
    void showMicPanel (bool show);
    double lastFrameMs = 0;
    juce::ComboBox presetBox;
    juce::MidiKeyboardComponent keyboard;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<Section> sections;
    juce::Rectangle<int> meterArea;
    float shownVoice = 0, shownGate = 0, shownOut = 0;
    bool shownVoiceMode = true, shownConnected = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasementalTalkBoxEditor)
};
