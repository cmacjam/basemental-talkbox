#include "PluginEditor.h"
#include "Presets.h"

using namespace juce;

static const std::map<String, String>& tooltipsText()
{
    static const std::map<String, String> t = {
        { ParamID::tone,        "MINI: filter cutoff.  DX: FM brightness (modulation index). The classic talker sound wants this fairly open." },
        { ParamID::character,   "MINI: filter resonance.  DX: modulator feedback (more = buzzier, saw-like)." },
        { ParamID::envAmt,      "How much the envelope opens the filter / FM brightness on each note." },
        { ParamID::detune,      "Second oscillator, slightly detuned, for a fatter lead." },
        { ParamID::sub,         "Sub-octave oscillator." },
        { ParamID::glide,       "Portamento between overlapping (legato) notes. Mono mode only." },
        { ParamID::vibrato,     "Delayed vibrato that fades in on held notes. In VOICE mode the mod wheel adds more." },
        { ParamID::attack,      "Envelope attack." },
        { ParamID::decay,       "Envelope decay." },
        { ParamID::sustain,     "Envelope sustain level." },
        { ParamID::release,     "Envelope release." },
        { ParamID::sensitivity, "Voice gate. Higher = opens on quieter talking/whispering. The synth only sounds while you're mouthing words." },
        { ParamID::response,    "Articulation smoothing. Low = crisp, snappy consonants. High = smoother, rounder vowels." },
        { ParamID::brightness,  "How much of your mouth's top end shapes the synth. Lower = darker, more muffled talker." },
        { ParamID::consonants,  "Your real S, T, K and breath sounds, picked up by the 'mic' and mixed in. This is what makes words intelligible." },
        { ParamID::vowel,       "VOWEL mode: mouth shape OO / OH / AH / EH / EE. The mod wheel sweeps it too, so you can talk with your left hand." },
        { ParamID::wah,         "VOWEL mode: how closed the mouth starts on each new phrase (a 'wow' / 'yeah' opening)." },
        { ParamID::talkTime,    "VOWEL mode: how long the mouth takes to open." },
        { ParamID::drive,       "Talk box driver overdrive. Push it for the gritty, compressed sound of a cranked box." },
        { ParamID::tube,        "Resonance of the plastic hose between the driver and your mouth." },
        { ParamID::leak,        "Synth heard straight from the box, not through your mouth." },
        { ParamID::doubler,     "Double-tracked stereo spread." },
        { ParamID::roomMix,     "Small room amount." },
        { ParamID::roomSize,    "Room size." },
        { ParamID::bendRange,   "Pitch bend range in semitones." },
        { ParamID::tune,        "Master tuning (A4)." },
        { ParamID::volume,      "Output level." },
    };
    return t;
}

BasementalTalkBoxEditor::BasementalTalkBoxEditor (BasementalTalkBoxProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      keyboard (p.keyboardState, MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    using namespace ParamID;
    addKnob (tone, "CUTOFF"); addKnob (character, "RESO"); addKnob (envAmt, "CONTOUR"); addKnob (detune, "DETUNE");
    addKnob (sub, "SUB"); addKnob (glide, "GLIDE"); addKnob (vibrato, "VIBRATO");
    addSwitch (poly, "POLY");
    addKnob (attack, "ATTACK"); addKnob (decay, "DECAY"); addKnob (sustain, "SUSTAIN"); addKnob (release, "RELEASE");
    addKnob (sensitivity, "SENSITIVITY"); addKnob (response, "RESPONSE"); addKnob (brightness, "BRIGHTNESS"); addKnob (consonants, "CONSONANTS");
    addKnob (vowel, "VOWEL"); addKnob (wah, "WAH"); addKnob (talkTime, "TALK TIME");
    addKnob (drive, "DRIVE"); addKnob (tube, "TUBE"); addKnob (leak, "LEAK");
    addSwitch (doublerOn, "ON"); addKnob (doubler, "AMOUNT");
    addSwitch (roomOn, "ON"); addKnob (roomMix, "MIX"); addKnob (roomSize, "SIZE");
    addKnob (bendRange, "BEND"); addKnob (tune, "TUNE"); addKnob (volume, "VOLUME");

    content.addAndMakeVisible (stage);
    micButton.setTooltip ("Pick your microphone or audio interface (Bluetooth works too)");
    micButton.onClick = [this] { showMicPanel (micPanel == nullptr); };
    content.addAndMakeVisible (micButton);

    for (auto* b : { &miniButton, &dxButton, &voiceButton, &vowelButton })
    {
        b->setClickingTogglesState (false);
        content.addAndMakeVisible (*b);
    }
    miniButton.onClick  = [this] { setChoice (ParamID::synthModel, 0); };
    dxButton.onClick    = [this] { setChoice (ParamID::synthModel, 1); };
    voiceButton.onClick = [this] { setChoice (ParamID::mouthMode, 0); };
    vowelButton.onClick = [this] { setChoice (ParamID::mouthMode, 1); };
    miniButton.setTooltip ("MINI: three-oscillator analog-style mono synth (saws + sub) into a 4-pole ladder filter.");
    dxButton.setTooltip ("DX: bright, buzzy two-operator FM with feedback, the 80s digital talker sound.");
    voiceButton.setTooltip ("VOICE: your mic on the side chain is the mouth. Mouth or whisper the words while you play.");
    vowelButton.setTooltip ("VOWEL: no mic needed. The VOWEL knob and the mod wheel shape the mouth.");

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setTextWhenNothingSelected ("Presets");
    presetBox.onChange = [this] {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0) proc.setCurrentProgram (idx);
    };
    presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);
    content.addAndMakeVisible (presetBox);

    keyboard.setAvailableRange (36, 96);
    keyboard.setLowestVisibleKey (36);
    keyboard.setOctaveForMiddleC (4);
    keyboard.setColour (MidiKeyboardComponent::keyDownOverlayColourId, Colours_::accent.withAlpha (0.55f));
    keyboard.setColour (MidiKeyboardComponent::mouseOverKeyOverlayColourId, Colours_::accent.withAlpha (0.2f));
    keyboard.setColour (MidiKeyboardComponent::whiteNoteColourId, Colour (0xfff4f1ea));
    keyboard.setColour (MidiKeyboardComponent::blackNoteColourId, Colour (0xff141216));
    keyboard.setColour (MidiKeyboardComponent::keySeparatorLineColourId, Colour (0xff857c8a));
    keyboard.setColour (MidiKeyboardComponent::shadowColourId, Colours::black.withAlpha (0.4f));
    keyboard.setWantsKeyboardFocus (false);
    content.addAndMakeVisible (keyboard);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) kBaseW / kBaseH);
    setResizeLimits (kBaseW * 6 / 10, kBaseH * 6 / 10, kBaseW * 2, kBaseH * 2);
    setSize (kBaseW * 9 / 10, kBaseH * 9 / 10);

    layoutContent();
    startTimerHz (30);
    timerCallback();

    // first run of the app (or a fresh plug-in in mic mode): show where to pick the mic
    if (proc.usingMic() && ! proc.hasChosenMic())
        MessageManager::callAsync ([sp = Component::SafePointer<BasementalTalkBoxEditor> (this)] { if (sp) sp->showMicPanel (true); });
}

void BasementalTalkBoxEditor::showMicPanel (bool show)
{
    if (show && micPanel == nullptr)
    {
        micPanel = std::make_unique<MicSetupPanel> (proc);
        micPanel->setBounds (0, 0, kBaseW, kBaseH);
        Component::SafePointer<BasementalTalkBoxEditor> safeThis (this);
        micPanel->onClose = [safeThis] {
            MessageManager::callAsync ([safeThis] { if (safeThis != nullptr) safeThis->showMicPanel (false); });
        };
        content.addAndMakeVisible (*micPanel);
        micPanel->toFront (true);
    }
    else if (! show && micPanel != nullptr)
    {
        if (! proc.hasChosenMic() && proc.usingMic())   // closing without picking = keep the system default
            proc.chooseMic ({}, {}, MicBridge::kAutoChannel);
        micPanel.reset();
    }
}

BasementalTalkBoxEditor::~BasementalTalkBoxEditor()
{
    stopTimer();
    micPanel.reset();
    setLookAndFeel (nullptr);
}

BasementalTalkBoxEditor::Knob& BasementalTalkBoxEditor::addKnob (const char* paramId, const String& name)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, 84, 18);
    k->slider.setRotaryParameters (MathConstants<float>::pi * 1.2f, MathConstants<float>::pi * 2.8f, true);
    k->slider.setPopupDisplayEnabled (false, false, nullptr);
    k->slider.setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
    k->slider.setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
    k->slider.setColour (Slider::textBoxTextColourId, Colours_::cream);
    auto it = tooltipsText().find (paramId);
    if (it != tooltipsText().end()) k->slider.setTooltip (it->second);
    k->label.setText (name, dontSendNotification);
    k->label.setJustificationType (Justification::centred);
    k->label.setFont (FontOptions (11.5f, Font::bold));
    k->label.setColour (Label::textColourId, Colours_::textDim);
    content.addAndMakeVisible (k->slider);
    content.addAndMakeVisible (k->label);
    k->att = std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k->slider);
    auto& ref = *k;
    knobById[paramId] = k.get();
    knobs.push_back (std::move (k));
    return ref;
}

BasementalTalkBoxEditor::Switch& BasementalTalkBoxEditor::addSwitch (const char* paramId, const String& name)
{
    auto s = std::make_unique<Switch>();
    s->button.setButtonText (name);
    content.addAndMakeVisible (s->button);
    s->att = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, paramId, s->button);
    auto& ref = *s;
    switchById[paramId] = s.get();
    switches.push_back (std::move (s));
    return ref;
}

void BasementalTalkBoxEditor::setChoice (const char* paramId, int index)
{
    if (auto* p = proc.apvts.getParameter (paramId))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
        p->endChangeGesture();
    }
}

void BasementalTalkBoxEditor::timerCallback()
{
    using namespace ParamID;
    const bool dx = proc.apvts.getRawParameterValue (synthModel)->load() > 0.5f;
    const bool voiceMode = proc.apvts.getRawParameterValue (mouthMode)->load() < 0.5f;
    miniButton.setToggleState (! dx, dontSendNotification);
    dxButton.setToggleState (dx, dontSendNotification);
    voiceButton.setToggleState (voiceMode, dontSendNotification);
    vowelButton.setToggleState (! voiceMode, dontSendNotification);

    knobById[tone]->label.setText (dx ? "BRIGHT" : "CUTOFF", dontSendNotification);
    knobById[character]->label.setText (dx ? "FEEDBACK" : "RESO", dontSendNotification);

    if (proc.getCurrentProgram() != presetBox.getSelectedItemIndex())
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), dontSendNotification);

    auto enable = [this] (std::initializer_list<const char*> ids, bool on)
    {
        for (auto* id : ids) if (auto it = knobById.find (id); it != knobById.end()) it->second->slider.setEnabled (on);
    };
    enable ({ sensitivity, brightness, consonants }, voiceMode);
    enable ({ vowel, wah, talkTime }, ! voiceMode);
    enable ({ glide }, proc.apvts.getRawParameterValue (poly)->load() < 0.5f);
    enable ({ doubler }, proc.apvts.getRawParameterValue (doublerOn)->load() > 0.5f);
    enable ({ roomMix, roomSize }, proc.apvts.getRawParameterValue (roomOn)->load() > 0.5f);

    // the stage
    const double now = Time::getMillisecondCounterHiRes();
    const float dt = lastFrameMs > 0 ? (float) jlimit (0.005, 0.1, (now - lastFrameMs) / 1000.0) : 1.0f / 30.0f;
    lastFrameMs = now;
    TalkBoxStage::Frame f;
    f.open = proc.getMouthOpen() * (voiceMode ? 1.0f : (proc.notesHeld() || proc.getOutputLevel() > 0.01f ? 1.0f : 0.0f));
    f.wide = proc.getMouthWide();
    f.out = proc.getOutputLevel();
    f.voice = proc.getVoiceLevel();
    f.notes = proc.notesHeld();
    f.voiceMode = voiceMode;
    f.vowelLabel = vowelName (proc.apvts.getRawParameterValue (vowel)->load());
    stage.update (f, dt);

    // mic button: what's connected, with a status light
    auto& mic = proc.getMic();
    String label;
    if (! proc.usingMic()) label = "SIDE CHAIN";
    else if (mic.getState() == MicBridge::State::Running) label = mic.getOpenName();
    else if (mic.getState() == MicBridge::State::Missing) label = "Waiting for mic...";
    else if (mic.getState() == MicBridge::State::Denied) label = "Mic blocked: allow access";
    else if (mic.getState() == MicBridge::State::Failed) label = "Mic problem: click";
    else label = "Choose mic";
    micButton.setButtonText (label);
    const bool ok = mic.getState() == MicBridge::State::Running;
    micButton.getProperties().set ("led", ! proc.usingMic() ? 3 : (ok ? 1 : (mic.getState() == MicBridge::State::Missing ? 2 : 0)));
    micButton.repaint();
}

void BasementalTalkBoxEditor::paint (Graphics& g) { g.fillAll (Colours_::tolex); }

void BasementalTalkBoxEditor::resized()
{
    const float scale = (float) getWidth() / (float) kBaseW;
    content.setBounds (0, 0, kBaseW, kBaseH);
    content.setTransform (AffineTransform::scale (scale));
}

void BasementalTalkBoxEditor::layoutContent()
{
    using namespace ParamID;
    sections.clear();

    // header: [MIC: device]  [MINI|DX]  [VOICE|VOWEL]  [presets]
    presetBox.setBounds (kBaseW - 20 - 230, 24, 230, 32);
    int x = kBaseW - 20 - 230 - 20;
    vowelButton.setBounds (x - 90, 24, 90, 32);
    voiceButton.setBounds (x - 180, 24, 90, 32);
    x -= 180 + 16;
    dxButton.setBounds (x - 70, 24, 70, 32);
    miniButton.setBounds (x - 140, 24, 70, 32);
    x -= 140 + 16;
    micButton.setBounds (x - 230, 24, 230, 32);
    miniButton.setConnectedEdges (Button::ConnectedOnRight);  dxButton.setConnectedEdges (Button::ConnectedOnLeft);
    voiceButton.setConnectedEdges (Button::ConnectedOnRight); vowelButton.setConnectedEdges (Button::ConnectedOnLeft);

    auto placeKnobs = [this] (Rectangle<int> area, std::initializer_list<const char*> ids, int labelH)
    {
        const int n = (int) ids.size();
        const int w = area.getWidth() / n;
        int i = 0;
        for (auto* id : ids)
        {
            auto cell = Rectangle<int> (area.getX() + i * w, area.getY(), w, area.getHeight());
            auto* k = knobById[id];
            k->label.setBounds (cell.removeFromTop (labelH));
            k->slider.setTextBoxStyle (Slider::TextBoxBelow, false, jmin (84, w - 4), 18);
            k->slider.setBounds (cell.reduced (2, 0));
            ++i;
        }
    };
    auto addSection = [this] (const String& title, Rectangle<int> r) { sections.push_back ({ title, r }); return r; };
    auto inner = [] (Rectangle<int> r) { return r.withTrimmedTop (30).reduced (8, 4); };
    auto placeSwitch = [this] (const char* id, Rectangle<int> sec)
    {
        switchById[id]->button.setBounds (sec.getRight() - 56, sec.getY() + 5, 50, 20);
    };

    // left: the stage (singer, hose, box)
    stage.setBounds (20, 104, (int) TalkBoxStage::kW, (int) TalkBoxStage::kH);

    // right column: SYNTH, then MOUTH | VOWEL
    const int rx = 452, rw = kBaseW - 20 - rx, rh = 170;
    auto syn = addSection ("SYNTH", { rx, 104, rw, rh });
    placeKnobs (inner (syn), { tone, character, envAmt, detune, sub, glide, vibrato }, 16);
    placeSwitch (poly, syn);
    const int r2y = 104 + rh + 12;
    auto mo = addSection ("MOUTH  (YOUR VOICE)", { rx, r2y, 392, rh });
    placeKnobs (inner (mo), { sensitivity, response, brightness, consonants }, 16);
    auto vw = addSection ("VOWEL  (NO MIC)", { rx + 392 + 12, r2y, rw - 392 - 12, rh });
    placeKnobs (inner (vw), { vowel, wah, talkTime }, 16);

    // bottom row: ENVELOPE | THE BOX | DOUBLER | ROOM | MASTER
    const int r3y = 104 + (int) TalkBoxStage::kH + 12, r3h = 160;
    int bx = 20;
    auto next = [&] (int w) { Rectangle<int> r (bx, r3y, w, r3h); bx += w + 12; return r; };
    auto env = addSection ("ENVELOPE", next (310));
    placeKnobs (inner (env), { attack, decay, sustain, release }, 16);
    auto box = addSection ("THE BOX", next (232));
    placeKnobs (inner (box), { drive, tube, leak }, 16);
    auto db = addSection ("DOUBLER", next (150));
    placeKnobs (inner (db), { doubler }, 16);
    placeSwitch (doublerOn, db);
    auto rm = addSection ("ROOM", next (176));
    placeKnobs (inner (rm), { roomMix, roomSize }, 16);
    placeSwitch (roomOn, rm);
    auto ms = addSection ("MASTER", Rectangle<int> (bx, r3y, kBaseW - 20 - bx, r3h));
    placeKnobs (inner (ms), { bendRange, tune, volume }, 16);
    meterArea = {};

    const int ky = r3y + r3h + 16;
    keyboard.setBounds (20, ky, kBaseW - 40, kBaseH - ky - 14);
    keyboard.setKeyWidth ((float) (kBaseW - 40) / 36.0f);   // 36 white keys in C2..C7
    if (micPanel) micPanel->setBounds (0, 0, kBaseW, kBaseH);
}

void BasementalTalkBoxEditor::paintContent (Graphics& g)
{
    const auto W = (float) kBaseW;
    g.fillAll (Colours_::tolex);
    Random rng (11);
    g.setColour (Colours::white.withAlpha (0.018f));
    for (int i = 0; i < 2800; ++i)
        g.fillRect ((float) rng.nextInt (kBaseW), (float) rng.nextInt (kBaseH), 1.0f, 1.0f);

    // header
    g.setColour (Colours_::accent);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("B A S E M E N T A L", 24, 16, 300, 16, Justification::centredLeft);
    g.setColour (Colours_::cream);
    g.setFont (FontOptions (30.0f, Font::bold));
    g.drawText (JucePlugin_Name, 22, 32, 360, 36, Justification::centredLeft);

    // gold rail + magenta stripe
    Rectangle<float> rail (0, 80, W, 10);
    g.setGradientFill (ColourGradient (Colour (0xfff3d27a), 0, rail.getY(), Colour (0xff9c7424), 0, rail.getBottom(), false));
    g.fillRect (rail);
    g.setColour (Colours_::feltRed);
    g.fillRect (0.0f, rail.getBottom(), W, 3.0f);

    for (auto& s : sections)
    {
        auto r = s.area.toFloat();
        g.setColour (Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (r.translated (0, 2), 8.0f);
        g.setColour (Colours_::panel);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (Colours_::panelEdge);
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (Colours_::cream);
        g.setFont (FontOptions (13.0f, Font::bold));
        g.drawText (s.title, r.getX() + 12, r.getY() + 6, 220, 18, Justification::centredLeft);
        g.setColour (Colours_::accent.withAlpha (0.8f));
        g.fillRect (r.getX() + 12, r.getY() + 26, 26.0f, 2.0f);
    }

    // stage frame
    {
        auto r = stage.getBounds().toFloat().expanded (1.0f);
        g.setColour (Colours_::accent.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 9.0f, 1.2f);
    }

    // mic button status light
    {
        const int led = (int) micButton.getProperties().getWithDefault ("led", 0);
        const auto b = micButton.getBounds().toFloat();
        const Colour c = led == 1 ? Colour (0xff53e07a) : led == 2 ? Colours_::accent : led == 3 ? Colours_::tubeClear : Colour (0xffff4a4a);
        g.setColour (c.withAlpha (0.25f)); g.fillEllipse (b.getX() - 19, b.getCentreY() - 7, 14, 14);
        g.setColour (c); g.fillEllipse (b.getX() - 16, b.getCentreY() - 4, 8, 8);
        g.setColour (Colours_::textDim); g.setFont (FontOptions (9.5f, Font::bold));
        g.drawText ("VOICE IN", b.withY (b.getY() - 14).withHeight (12), Justification::centredLeft);
    }

    // keyboard frame
    auto kb = keyboard.getBounds().toFloat().expanded (6, 8);
    g.setColour (Colour (0xff0d0b10));
    g.fillRoundedRectangle (kb, 6.0f);
    g.setColour (Colours_::feltRed);
    g.fillRect (kb.getX() + 6, kb.getY() + 4, kb.getWidth() - 12, 3.0f);
}
