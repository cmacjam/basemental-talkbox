#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

// Voice input
//   MIC (default): the plug-in opens the microphone itself, like plugging a mic into a real talk box.
//                  Any input device works: built-in, USB interface, Bluetooth headset/AirPods, iPhone.
//   SIDE CHAIN   : the DAW routes a track into the plug-in (buses below).
//
// Buses
//   in 0  "Input"              main input. Standalone app: your mic / interface input.
//   in 1  "Voice (Side Chain)" mono side chain carrying your voice:
//            Logic / AU hosts -> the plug-in's Side Chain menu
//            Pro Tools (AAX)  -> the plug-in's Key Input menu
//            VST3 / LV2 hosts -> the side-chain / aux input (Reaper pins 3-4, Bitwig, Ardour, Cubase...)
//   out 0 "Output"             stereo
// The voice is whatever arrives on either input, so every host routing works.
BasementalTalkBoxProcessor::BasementalTalkBoxProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input", juce::AudioChannelSet::stereo(), true)
                        .withInput  ("Voice (Side Chain)", juce::AudioChannelSet::mono(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BasementalTalkBox", createParameterLayout())
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            raw[rp->getParameterID()] = apvts.getRawParameterValue (rp->getParameterID());
    apvts.addParameterListener (ParamID::voiceSource, this);
}

void BasementalTalkBoxProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }

void BasementalTalkBoxProcessor::handleAsyncUpdate()
{
    if (usingMic())
    {
        const auto& st = apvts.state;
        const auto type = st.getProperty ("micType", "").toString();
        const auto name = st.getProperty ("micName", "").toString();
        const int ch = (int) st.getProperty ("micChannel", MicBridge::kAutoChannel);
        if (mic.getState() != MicBridge::State::Running || name != mic.getWantedName() || type != mic.getWantedType())
            mic.setWanted (type, name, ch);
        else
            mic.setChannelMode (ch);
    }
    else
        mic.close();
}

void BasementalTalkBoxProcessor::chooseMic (const juce::String& type, const juce::String& name, int channel)
{
    apvts.state.setProperty ("micType", type, nullptr);
    apvts.state.setProperty ("micName", name, nullptr);
    apvts.state.setProperty ("micChannel", channel, nullptr);
    apvts.state.setProperty ("micChosen", true, nullptr);
    if (auto* p = apvts.getParameter (ParamID::voiceSource)) p->setValueNotifyingHost (0.0f);
    mic.setWanted (type, name, channel);
}

void BasementalTalkBoxProcessor::setMicChannel (int channel)
{
    apvts.state.setProperty ("micChannel", channel, nullptr);
    mic.setChannelMode (channel);
}

bool BasementalTalkBoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    auto okIn = [] (const juce::AudioChannelSet& s)
    { return s.isDisabled() || s == juce::AudioChannelSet::mono() || s == juce::AudioChannelSet::stereo(); };
    if (! okIn (layouts.getMainInputChannelSet())) return false;
    if (layouts.inputBuses.size() > 1 && ! okIn (layouts.getChannelSet (true, 1))) return false;
    return true;
}

void BasementalTalkBoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto n = (size_t) std::max (512, samplesPerBlock);
    voiceBuf.assign (n, 0.0f); outL.assign (n, 0.0f); outR.assign (n, 0.0f); micBuf.assign (n, 0.0f);
    mic.prepare (sampleRate);
    triggerAsyncUpdate();   // open the mic from the message thread (never from the audio thread)
    updateEngineParams();
    engine.prepare (sampleRate, samplesPerBlock);
    keyboardState.reset();
    sustainDown = false; heldBySustain.fill (false);
}

void BasementalTalkBoxProcessor::updateEngineParams()
{
    using namespace ParamID;
    btb::EngineParams e;
    e.synth.dx        = P (synthModel) > 0.5f;
    e.synth.poly      = P (poly) > 0.5f;
    e.synth.tone      = P (tone);
    e.synth.character = P (character);
    e.synth.envAmt    = P (envAmt);
    e.synth.detune    = P (detune);
    e.synth.sub       = P (sub);
    e.synth.glide     = P (glide);
    e.synth.vibrato   = P (vibrato);
    e.synth.attack    = P (attack);
    e.synth.decay     = P (decay);
    e.synth.sustain   = P (sustain);
    e.synth.release   = P (release);
    e.synth.bendRange = std::round (P (bendRange));
    e.synth.tuneA4    = P (tune);

    e.mouth.useVoice    = P (mouthMode) < 0.5f;
    e.mouth.sensitivity = P (sensitivity);
    e.mouth.response    = P (response);
    e.mouth.brightness  = P (brightness);
    e.mouth.vowel       = P (vowel);
    e.mouth.wah         = P (wah);
    e.mouth.talkTime    = P (talkTime);
    e.consonants        = P (consonants);

    e.drive     = P (drive);
    e.tube      = P (tube);
    e.leak      = P (leak);
    e.doublerOn = P (doublerOn) > 0.5f;
    e.doubler   = P (doubler);
    e.roomOn    = P (roomOn) > 0.5f;
    e.roomMix   = P (roomMix);
    e.roomSize  = P (roomSize);
    e.volumeDb  = P (volume);
    engine.setParams (e);
}

void BasementalTalkBoxProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        heldBySustain[(size_t) m.getNoteNumber()] = false;
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        if (sustainDown) heldBySustain[(size_t) m.getNoteNumber()] = true;
        else engine.noteOff (m.getNoteNumber());
    }
    else if (m.isSustainPedalOn())  sustainDown = true;
    else if (m.isSustainPedalOff())
    {
        sustainDown = false;
        for (int n = 0; n < 128; ++n)
            if (heldBySustain[(size_t) n]) { heldBySustain[(size_t) n] = false; engine.noteOff (n); }
    }
    else if (m.isPitchWheel())
        engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType (1))
        engine.modWheel ((float) m.getControllerValue() / 127.0f);
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        engine.allNotesOff(); heldBySustain.fill (false);
    }
}

void BasementalTalkBoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    updateEngineParams();

    // Input and output share channel memory, so take a copy of the voice first (mono sum).
    if ((int) voiceBuf.size() < numSamples)
    {
        // host gave a bigger block than announced: grow once (rare)
        voiceBuf.resize ((size_t) numSamples); outL.resize ((size_t) numSamples); outR.resize ((size_t) numSamples);
        micBuf.resize ((size_t) numSamples);
    }
    std::fill (voiceBuf.begin(), voiceBuf.begin() + numSamples, 0.0f);
    int connected = 0;
    for (int b = 0; b < getBusCount (true); ++b)
    {
        auto* bus = getBus (true, b);
        if (bus == nullptr || ! bus->isEnabled()) continue;
        auto inBus = getBusBuffer (buffer, true, b);
        const int ch = inBus.getNumChannels();
        if (ch == 0) continue;
        ++connected;
        const float scale = 1.0f / (float) ch;
        for (int c = 0; c < ch; ++c)
        {
            const float* src = inBus.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i) voiceBuf[(size_t) i] += src[i] * scale;
        }
    }
    // Direct mic: when it's running it is the voice (the DAW inputs are ignored, so nothing doubles up).
    // The standalone app always uses it. If the mic isn't available, fall back to whatever the DAW sends.
    const bool micMode = isStandalone() || P (ParamID::voiceSource) < 0.5f;
    if (micMode && mic.isRunning())
    {
        mic.read (micBuf.data(), numSamples);
        const float g = juce::Decibels::decibelsToGain (P (ParamID::micGain));
        for (int i = 0; i < numSamples; ++i) voiceBuf[(size_t) i] = micBuf[(size_t) i] * g;
        connected = 1;
    }
    else if (micMode)
    {
        mic.read (micBuf.data(), numSamples);   // keeps the reader in sync while (re)connecting
        if (isStandalone()) { std::fill (voiceBuf.begin(), voiceBuf.begin() + numSamples, 0.0f); connected = 0; }
    }

    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i) inPeak = std::max (inPeak, std::abs (voiceBuf[(size_t) i]));
    voiceConnected.store (connected > 0);

    auto renderRange = [&] (int start, int len)
    {
        if (len > 0) engine.process (voiceBuf.data() + start, outL.data() + start, outR.data() + start, len);
    };
    int pos = 0;
    for (const auto metadata : midi)
    {
        const int t = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderRange (pos, t - pos);
        pos = t;
        handleMidi (metadata.getMessage());
    }
    renderRange (pos, numSamples - pos);

    auto outBus = getBusBuffer (buffer, false, 0);
    if (outBus.getNumChannels() >= 2)
    {
        outBus.copyFrom (0, 0, outL.data(), numSamples);
        outBus.copyFrom (1, 0, outR.data(), numSamples);
        for (int ch = 2; ch < outBus.getNumChannels(); ++ch) outBus.clear (ch, 0, numSamples);
    }
    else if (outBus.getNumChannels() == 1)
    {
        auto* o = outBus.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i) o[i] = 0.5f * (outL[(size_t) i] + outR[(size_t) i]);
    }
    // never pass the raw voice through: clear any extra channels beyond the output bus
    for (int ch = outBus.getNumChannels(); ch < buffer.getNumChannels(); ++ch) buffer.clear (ch, 0, numSamples);

    voiceLevel.store (std::max (inPeak, voiceLevel.load() * 0.8f));
    gate.store (engine.gateValue());
    outLevel.store (engine.outputLevel());
    activeVoices.store (engine.activeVoices());
    mouthOpen.store (engine.mouthOpen());
    mouthWide.store (engine.mouthWide());
    held.store (engine.heldNotes() > 0);
}

//==============================================================================
int BasementalTalkBoxProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String BasementalTalkBoxProcessor::getProgramName (int index)
{
    const auto& ps = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) ps.size()) ? juce::String (ps[(size_t) index].name) : juce::String();
}

void BasementalTalkBoxProcessor::setCurrentProgram (int index)
{
    const auto& ps = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) ps.size())) return;
    currentProgram = index;

    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID() != ParamID::voiceSource && rp->getParameterID() != ParamID::micGain)
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : ps[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void BasementalTalkBoxProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BasementalTalkBoxProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            apvts.replaceState (vt);
            triggerAsyncUpdate();   // reopen the project's mic
        }
}

juce::AudioProcessorEditor* BasementalTalkBoxProcessor::createEditor()
{
    return new BasementalTalkBoxEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasementalTalkBoxProcessor();
}
