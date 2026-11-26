#include "MicSetupPanel.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
 static juce::AudioDeviceManager* standaloneDeviceManager()
 {
     if (auto* h = juce::StandalonePluginHolder::getInstance()) return &h->deviceManager;
     return nullptr;
 }
 static void showStandaloneAudioSettings()
 {
     if (auto* h = juce::StandalonePluginHolder::getInstance()) h->showAudioSettingsDialog();
 }
#else
 static juce::AudioDeviceManager* standaloneDeviceManager() { return nullptr; }
 static void showStandaloneAudioSettings() {}
#endif

using namespace juce;

MicSetupPanel::MicSetupPanel (BasementalTalkBoxProcessor& p) : proc (p)
{
    standalone = proc.isStandalone();
    setInterceptsMouseClicks (true, true);

    for (auto* b : { &micSourceButton, &sideChainButton }) { b->setClickingTogglesState (false); addAndMakeVisible (*b); }
    micSourceButton.setConnectedEdges (Button::ConnectedOnRight);
    sideChainButton.setConnectedEdges (Button::ConnectedOnLeft);
    micSourceButton.onClick = [this] { setSource (true); };
    sideChainButton.onClick = [this] { setSource (false); };
    sideChainButton.setVisible (! standalone);

    list.setRowHeight (38);
    list.setColour (ListBox::backgroundColourId, Colour (0xff141118));
    list.setColour (ListBox::outlineColourId, Colours_::panelEdge);
    list.setOutlineThickness (1);
    addAndMakeVisible (list);

    refreshButton.onClick = [this] { proc.getMic().refreshDevices(); refresh(); };
    addAndMakeVisible (refreshButton);

    gain.setSliderStyle (Slider::LinearHorizontal);
    gain.setTextBoxStyle (Slider::TextBoxRight, false, 70, 22);
    gain.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    gain.setColour (Slider::textBoxTextColourId, Colours_::cream);
    gain.setColour (Slider::trackColourId, Colours_::accent);
    gain.setColour (Slider::thumbColourId, Colours_::cream);
    gainAtt = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ParamID::micGain, gain);
    addAndMakeVisible (gain);

    autoLevelButton.setTooltip ("Sing or talk normally for 3 seconds: the mic gain is set for you.");
    autoLevelButton.onClick = [this] {
        calibrateUntil = Time::getMillisecondCounterHiRes() + 3000.0; calibratePeak = 0.0f;
        autoLevelButton.setButtonText ("Listening...");
    };
    addAndMakeVisible (autoLevelButton);

    doneButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible (doneButton);

    if (standalone)
    {
        outputLabel.setText ("SPEAKERS / HEADPHONES", dontSendNotification);
        outputLabel.setFont (FontOptions (11.5f, Font::bold));
        outputLabel.setColour (Label::textColourId, Colours_::textDim);
        addAndMakeVisible (outputLabel);
        outputBox.onChange = [this] {
            if (auto* dm = standaloneDeviceManager())
            {
                auto setup = dm->getAudioDeviceSetup();
                setup.outputDeviceName = outputBox.getText();
                setup.inputDeviceName = {};            // the talk box opens the mic itself
                setup.useDefaultInputChannels = false;
                setup.inputChannels.clear();
                dm->setAudioDeviceSetup (setup, true);
            }
        };
        addAndMakeVisible (outputBox);
        moreOutputButton.setTooltip ("Audio driver, buffer size, MIDI keyboards...");
        moreOutputButton.onClick = [] { showStandaloneAudioSettings(); };
        addAndMakeVisible (moreOutputButton);
    }

    proc.getMic().onChange = [sp = Component::SafePointer<MicSetupPanel> (this)] { if (sp) sp->refresh(); };
    refresh();
    startTimerHz (30);
}

MicSetupPanel::~MicSetupPanel()
{
    proc.getMic().onChange = nullptr;
    list.setModel (nullptr);
}

Rectangle<int> MicSetupPanel::card() const
{
    return Rectangle<int> (kCardW, kCardH).withCentre (getLocalBounds().getCentre());
}

void MicSetupPanel::setSource (bool micDirect)
{
    if (auto* p = proc.apvts.getParameter (ParamID::voiceSource))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (micDirect ? 0.0f : 1.0f);
        p->endChangeGesture();
    }
    refresh();
}

void MicSetupPanel::refresh()
{
    auto& mic = proc.getMic();
    rows.clearQuick();
    rows.add ({ {}, {}, false, false, false, false, false, true });   // "System default"
    for (auto& d : mic.getDevices())
        rows.add ({ d.type, d.name, d.bluetooth, d.usb, d.builtIn, d.wireless, d.isDefault, false });
    list.updateContent();
    list.repaint();

    const bool micMode = proc.usingMic();
    micSourceButton.setToggleState (micMode, dontSendNotification);
    sideChainButton.setToggleState (! micMode, dontSendNotification);
    list.setEnabled (micMode);
    gain.setEnabled (micMode);
    autoLevelButton.setEnabled (micMode);

    if (standalone)
        if (auto* dm = standaloneDeviceManager())
            if (auto* t = dm->getCurrentDeviceTypeObject())
            {
                outputBox.clear (dontSendNotification);
                const auto outs = t->getDeviceNames (false);
                for (int i = 0; i < outs.size(); ++i) outputBox.addItem (outs[i], i + 1);
                outputBox.setText (dm->getAudioDeviceSetup().outputDeviceName, dontSendNotification);
            }

    rebuildChannelChips();
    resized();
    repaint();
}

void MicSetupPanel::rebuildChannelChips()
{
    chips.clear();
    const auto names = proc.getMic().getChannelNames();
    if (! proc.usingMic() || names.size() <= 1) return;

    const int mode = proc.getMic().getChannelMode();
    auto add = [&] (const String& text, int value, const String& tip)
    {
        auto* b = chips.add (new TextButton (text));
        b->setTooltip (tip);
        b->setClickingTogglesState (false);
        b->setToggleState (mode == value, dontSendNotification);
        b->onClick = [this, value] { proc.setMicChannel (value); rebuildChannelChips(); resized(); };
        addAndMakeVisible (b);
    };
    add ("AUTO", MicBridge::kAutoChannel, "Follows whichever input you're singing into");
    for (int i = 0; i < juce::jmin (8, names.size()); ++i)
        add (String (i + 1), i, names[i]);
    add ("MIX", MicBridge::kMixChannels, "All inputs mixed together");
}

void MicSetupPanel::resized()
{
    auto c = card().reduced (24);
    c.removeFromTop (56);                                   // title + subtitle
    auto src = c.removeFromTop (34);
    micSourceButton.setBounds (src.removeFromLeft (180));
    sideChainButton.setBounds (src.removeFromLeft (180));
    c.removeFromTop (14);

    auto footer = c.removeFromBottom (38);
    doneButton.setBounds (footer.removeFromRight (110).reduced (0, 2));
    c.removeFromBottom (8);

    auto left = c.removeFromLeft (370);
    auto listHead = left.removeFromTop (22);
    refreshButton.setBounds (listHead.removeFromRight (74).withHeight (20));
    left.removeFromTop (4);
    if (standalone)
    {
        auto out = left.removeFromBottom (56);
        outputLabel.setBounds (out.removeFromTop (20));
        auto row = out.removeFromTop (30);
        moreOutputButton.setBounds (row.removeFromRight (70));
        row.removeFromRight (6);
        outputBox.setBounds (row);
        left.removeFromBottom (8);
    }
    list.setBounds (left);

    auto right = c.withTrimmedLeft (22);
    right.removeFromTop (22 + 4);
    // level meter area painted at right top (y .. +36)
    right.removeFromTop (44);
    // channel chips
    if (! chips.isEmpty())
    {
        right.removeFromTop (20);
        auto row = right.removeFromTop (28);
        const int w = juce::jmin (44, row.getWidth() / chips.size());
        for (auto* b : chips) b->setBounds (row.removeFromLeft (w).reduced (2, 0));
        right.removeFromTop (12);
    }
    right.removeFromTop (20);
    gain.setBounds (right.removeFromTop (28));
    right.removeFromTop (8);
    autoLevelButton.setBounds (right.removeFromTop (28).removeFromLeft (120));
}

void MicSetupPanel::timerCallback()
{
    auto& mic = proc.getMic();
    const float pk = mic.takePeak() * Decibels::decibelsToGain (proc.apvts.getRawParameterValue (ParamID::micGain)->load());
    level = pk > level ? pk : level * 0.86f;
    holdPeak = juce::jmax (pk, holdPeak * 0.995f);

    if (calibrateUntil > 0.0)
    {
        calibratePeak = juce::jmax (calibratePeak, pk);
        if (Time::getMillisecondCounterHiRes() > calibrateUntil)
        {
            calibrateUntil = 0.0;
            autoLevelButton.setButtonText ("Auto-level");
            if (auto* p = proc.apvts.getParameter (ParamID::micGain))
            {
                const float currentDb = proc.apvts.getRawParameterValue (ParamID::micGain)->load();
                const float rawPeak = calibratePeak / Decibels::decibelsToGain (currentDb);
                if (rawPeak > 1.0e-4f)
                {
                    const float newDb = jlimit (-12.0f, 40.0f, Decibels::gainToDecibels (0.3f / rawPeak));   // peaks at about -10 dBFS
                    p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (newDb)); p->endChangeGesture();
                }
            }
        }
    }

    if (mic.getStatus() != lastStatus || mic.getDevices().size() != lastDeviceCount)
    {
        lastStatus = mic.getStatus(); lastDeviceCount = mic.getDevices().size();
        refresh();
    }
    repaint (card());
}

void MicSetupPanel::mouseDown (const MouseEvent& e)
{
    if (! card().contains (e.getPosition()) && onClose) onClose();
}

static void drawDeviceIcon (Graphics& g, Rectangle<float> r, bool bluetooth, bool usb, bool wireless, Colour c)
{
    g.setColour (c);
    if (bluetooth)
    {
        Path p;   // the bluetooth rune
        const float x = r.getCentreX(), t = r.getY() + 2, b = r.getBottom() - 2, m = r.getCentreY(), s = r.getHeight() * 0.22f;
        p.startNewSubPath (x - s, m - s); p.lineTo (x + s, m + s); p.lineTo (x, b); p.lineTo (x, t); p.lineTo (x + s, m - s); p.lineTo (x - s, m + s);
        g.strokePath (p, PathStrokeType (1.6f, PathStrokeType::mitered, PathStrokeType::rounded));
        return;
    }
    if (usb)
    {
        Path p; const float x = r.getCentreX();
        p.startNewSubPath (x, r.getBottom() - 2); p.lineTo (x, r.getY() + 3);
        p.startNewSubPath (x, r.getCentreY() + 2); p.lineTo (x - 5, r.getCentreY() - 2); p.lineTo (x - 5, r.getCentreY() - 5);
        p.startNewSubPath (x, r.getCentreY() + 5); p.lineTo (x + 5, r.getCentreY() + 1); p.lineTo (x + 5, r.getCentreY() - 3);
        g.strokePath (p, PathStrokeType (1.5f));
        g.fillEllipse (x - 2.5f, r.getY() + 1, 5, 5);
        return;
    }
    // microphone
    const auto cap = Rectangle<float> (r.getWidth() * 0.38f, r.getHeight() * 0.55f).withCentre ({ r.getCentreX(), r.getY() + r.getHeight() * 0.36f });
    g.drawRoundedRectangle (cap, cap.getWidth() * 0.5f, 1.5f);
    Path cup; cup.addCentredArc (r.getCentreX(), cap.getCentreY() + 1, cap.getWidth() * 0.85f, cap.getHeight() * 0.62f, 0, MathConstants<float>::pi * 0.5f, MathConstants<float>::pi * 1.5f, true);
    g.strokePath (cup, PathStrokeType (1.4f));
    g.drawLine (r.getCentreX(), cap.getBottom() + 3, r.getCentreX(), r.getBottom() - 1, 1.4f);
    if (wireless)
    {
        Path w; w.addCentredArc (r.getRight() - 3, r.getY() + 4, 4, 4, 0, -0.9f, 0.9f, true);
        g.strokePath (w, PathStrokeType (1.2f));
    }
}

void MicSetupPanel::paintListBoxItem (int row, Graphics& g, int w, int h, bool)
{
    if (! isPositiveAndBelow (row, rows.size())) return;
    const auto& r = rows.getReference (row);
    auto& mic = proc.getMic();
    const bool chosen = r.systemDefault ? mic.getWantedName().isEmpty() && proc.usingMic()
                                        : (r.name == mic.getWantedName() && proc.usingMic());
    const bool live = ! r.systemDefault && r.name == mic.getOpenName();

    if (chosen) { g.setColour (Colours_::accent.withAlpha (0.16f)); g.fillRect (0, 0, w, h); g.setColour (Colours_::accent); g.fillRect (0, 0, 3, h); }
    g.setColour (Colours_::panelEdge.withAlpha (0.6f));
    g.drawHorizontalLine (h - 1, 8.0f, (float) w - 8.0f);

    drawDeviceIcon (g, { 12.0f, 8.0f, 20.0f, (float) h - 16.0f }, r.bluetooth, r.usb, r.wireless, chosen ? Colours_::accent : Colours_::textDim);

    String name = r.systemDefault ? "System default" : r.name;
    if (r.systemDefault && mic.getWantedName().isEmpty() && mic.getOpenName().isNotEmpty()) name << "  (" << mic.getOpenName() << ")";
    g.setColour (Colours_::cream);
    g.setFont (FontOptions (13.5f, chosen ? Font::bold : Font::plain));
    g.drawFittedText (name, 42, 3, w - 150, h / 2 + 4, Justification::centredLeft, 1);

    // badges
    StringArray badges;
    if (r.systemDefault) badges.add ("FOLLOWS YOUR OS SETTING");
    if (r.bluetooth) badges.add ("BLUETOOTH");
    else if (r.wireless) badges.add ("WIRELESS");
    if (r.usb) badges.add ("USB");
    if (r.builtIn) badges.add ("BUILT-IN");
    if (r.isDefault) badges.add ("DEFAULT");
    float bx = 42.0f;
    g.setFont (FontOptions (9.0f, Font::bold));
    for (auto& b : badges)
    {
        const float bw = GlyphArrangement::getStringWidth (g.getCurrentFont(), b) + 10.0f;
        const auto col = b == "BLUETOOTH" || b == "WIRELESS" ? Colour (0xff4fa3ff) : (b == "USB" ? Colour (0xff6fd08c) : Colours_::textDim);
        g.setColour (col.withAlpha (0.18f)); g.fillRoundedRectangle (bx, (float) h - 17.0f, bw, 13.0f, 3.0f);
        g.setColour (col); g.drawText (b, Rectangle<float> (bx, (float) h - 17.0f, bw, 13.0f), Justification::centred);
        bx += bw + 5.0f;
    }

    if (live)
    {
        g.setColour (Colour (0xff53e07a));
        g.fillEllipse ((float) w - 22.0f, (float) h * 0.5f - 4.0f, 8.0f, 8.0f);
        g.setColour (Colours_::textDim); g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText ("LIVE", w - 64, 0, 38, h, Justification::centredRight);
    }
}

void MicSetupPanel::listBoxItemClicked (int row, const MouseEvent&)
{
    if (! isPositiveAndBelow (row, rows.size()) || ! proc.usingMic()) return;
    const auto& r = rows.getReference (row);
    proc.chooseMic (r.systemDefault ? String() : r.type, r.systemDefault ? String() : r.name, MicBridge::kAutoChannel);
    refresh();
}

void MicSetupPanel::paint (Graphics& g)
{
    g.fillAll (Colours::black.withAlpha (0.62f));
    const auto c = card().toFloat();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (c.translated (0, 6), 14.0f);
    g.setColour (Colours_::panel);
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (Colours_::accent.withAlpha (0.8f));
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.4f);

    auto inner = card().reduced (24);
    g.setColour (Colours_::cream);
    g.setFont (FontOptions (22.0f, Font::bold));
    g.drawText ("VOICE INPUT", inner.removeFromTop (30), Justification::centredLeft);
    g.setColour (Colours_::textDim);
    g.setFont (FontOptions (12.0f));
    g.drawText (proc.usingMic() ? "Plug in a mic, pick it, sing. Like a real talk box."
                                : "Your DAW sends a track into the plug-in's side-chain input.",
                card().reduced (24).withTrimmedTop (32).removeFromTop (16), Justification::centredLeft);

    auto body = card().reduced (24);
    body.removeFromTop (56 + 34 + 14);
    body.removeFromBottom (46);
    auto left = body.removeFromLeft (370);
    g.setColour (Colours_::textDim);
    g.setFont (FontOptions (11.5f, Font::bold));
    g.drawText ("MICROPHONES & INTERFACES", left.removeFromTop (22), Justification::centredLeft);

    auto right = body.withTrimmedLeft (22);
    auto& mic = proc.getMic();
    if (proc.usingMic())
    {
        g.drawText ("LEVEL", right.removeFromTop (22), Justification::centredLeft);
        right.removeFromTop (4);
        auto meter = right.removeFromTop (30).toFloat();
        g.setColour (Colour (0xff0f0d12)); g.fillRoundedRectangle (meter, 5);
        auto frac = [] (float v) { return jlimit (0.0f, 1.0f, (Decibels::gainToDecibels (v, -60.0f) + 60.0f) / 60.0f); };
        auto f = meter.reduced (3);
        // green to -12 dB, gold to -3 dB, red above: the sweet spot is gold
        const float L = frac (level);
        ColourGradient grad (Colour (0xff53e07a), f.getX(), 0, Colour (0xffff4a4a), f.getRight(), 0, false);
        grad.addColour (0.75, Colours_::accent);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (f.withWidth (f.getWidth() * L), 3);
        g.setColour (Colours_::cream.withAlpha (0.8f));
        g.fillRect (f.getX() + f.getWidth() * frac (holdPeak) - 1.0f, f.getY(), 2.0f, f.getHeight());
        g.setColour (Colours_::textDim.withAlpha (0.6f));
        for (float db : { -48.0f, -36.0f, -24.0f, -12.0f, -6.0f })
            g.drawVerticalLine ((int) (f.getX() + f.getWidth() * (db + 60.0f) / 60.0f), f.getY(), f.getY() + 4);
        right.removeFromTop (10);

        g.setColour (Colours_::textDim);
        g.setFont (FontOptions (11.5f, Font::bold));
        if (! chips.isEmpty())
        {
            String head = "INPUT CHANNEL";
            if (mic.getChannelMode() == MicBridge::kAutoChannel) head << "  (auto: input " << (mic.getActiveChannel() + 1) << ")";
            g.drawText (head, right.removeFromTop (20), Justification::centredLeft);
            right.removeFromTop (28 + 12);
        }
        g.drawText ("MIC GAIN", right.removeFromTop (20), Justification::centredLeft);
        right.removeFromTop (28 + 8 + 28 + 14);

        // status
        const auto st = mic.getState();
        const Colour led = st == MicBridge::State::Running ? Colour (0xff53e07a)
                         : st == MicBridge::State::Missing ? Colours_::accent : Colour (0xffff4a4a);
        g.setColour (led);
        g.fillEllipse ((float) right.getX(), (float) right.getY() + 4, 9, 9);
        g.setColour (Colours_::cream);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawFittedText (mic.getStatus().isEmpty() ? String ("Starting...") : mic.getStatus(),
                          right.removeFromTop (40).withTrimmedLeft (16), Justification::topLeft, 3);
        if (mic.getWarning().isNotEmpty())
        {
            g.setColour (Colour (0xff4fa3ff));
            g.setFont (FontOptions (11.5f));
            g.drawFittedText (mic.getWarning(), right.removeFromTop (64), Justification::topLeft, 5);
        }
    }
    else
    {
        g.setColour (Colours_::cream);
        g.setFont (FontOptions (12.5f));
        g.drawFittedText ("Logic / MainStage: open the Side Chain menu at the top of the plug-in window and pick your mic track.\n\n"
                          "Pro Tools: send the mic track to a bus, then choose that bus in the plug-in's Key Input menu.\n\n"
                          "Cubase, Studio One, Bitwig, Ableton, Reaper: route the mic track to the plug-in's side-chain input.\n\n"
                          "Tip: MICROPHONE mode skips all of this.",
                          right, Justification::topLeft, 14);
    }

    // footer tip
    auto foot = card().reduced (24).removeFromBottom (38);
    g.setColour (Colours_::textDim);
    g.setFont (FontOptions (11.0f));
    g.drawFittedText ("Least delay: a wired mic in your audio interface, or a USB mic. Bluetooth (AirPods, headsets) works too, with a little lag. "
                      "Unplug and replug any time: it reconnects by itself.",
                      foot.withTrimmedRight (124), Justification::centredLeft, 2);
}
