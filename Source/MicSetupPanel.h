// Raj T Vocal - "VOICE INPUT" setup panel: pick a mic in one click.
#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "BasementalLookAndFeel.h"

class MicSetupPanel : public juce::Component, private juce::Timer, private juce::ListBoxModel
{
public:
    explicit MicSetupPanel (BasementalTalkBoxProcessor&);
    ~MicSetupPanel() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;   // click outside the card closes it
    void refresh();                                      // rebuild lists from the device scan

    std::function<void()> onClose;
    static constexpr int kCardW = 720, kCardH = 540;

private:
    struct Row { juce::String type, name; bool bluetooth, usb, builtIn, wireless, isDefault, systemDefault; };

    int getNumRows() override { return rows.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void timerCallback() override;
    void rebuildChannelChips();
    void setSource (bool micDirect);
    juce::Rectangle<int> card() const;

    BasementalTalkBoxProcessor& proc;
    juce::Array<Row> rows;
    juce::ListBox list { "mics", this };
    juce::TextButton micSourceButton { "MICROPHONE" }, sideChainButton { "DAW SIDE CHAIN" };
    juce::TextButton refreshButton { "Rescan" }, autoLevelButton { "Auto-level" }, doneButton { "Done" };
    juce::Slider gain;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt;
    juce::OwnedArray<juce::TextButton> chips;
    juce::ComboBox outputBox;
    juce::TextButton moreOutputButton { "More..." };
    juce::Label outputLabel;

    float level = 0.0f, holdPeak = 0.0f;
    double calibrateUntil = 0.0; float calibratePeak = 0.0f;
    bool standalone = false;
    juce::String lastStatus;
    int lastDeviceCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MicSetupPanel)
};
