#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace ParamID
{
    // synth (the carrier)
    inline constexpr const char* synthModel = "synthModel";   // 0 = MINI, 1 = DX
    inline constexpr const char* poly       = "poly";
    inline constexpr const char* tone       = "tone";
    inline constexpr const char* character  = "character";
    inline constexpr const char* envAmt     = "envAmt";
    inline constexpr const char* detune     = "detune";
    inline constexpr const char* sub        = "sub";
    inline constexpr const char* glide      = "glide";
    inline constexpr const char* vibrato    = "vibrato";
    // envelope
    inline constexpr const char* attack     = "attack";
    inline constexpr const char* decay      = "decay";
    inline constexpr const char* sustain    = "sustain";
    inline constexpr const char* release    = "release";
    // mouth
    inline constexpr const char* mouthMode  = "mouthMode";    // 0 = VOICE (side chain), 1 = VOWEL (no mic)
    inline constexpr const char* sensitivity= "sensitivity";
    inline constexpr const char* response   = "response";
    inline constexpr const char* brightness = "brightness";
    inline constexpr const char* consonants = "consonants";
    inline constexpr const char* vowel      = "vowel";
    inline constexpr const char* wah        = "wah";
    inline constexpr const char* talkTime   = "talkTime";
    // the box
    inline constexpr const char* drive      = "drive";
    inline constexpr const char* tube       = "tube";
    inline constexpr const char* leak       = "leak";
    // output
    inline constexpr const char* doublerOn  = "doublerOn";
    inline constexpr const char* doubler    = "doubler";
    inline constexpr const char* roomOn     = "roomOn";
    inline constexpr const char* roomMix    = "roomMix";
    inline constexpr const char* roomSize   = "roomSize";
    inline constexpr const char* bendRange  = "bendRange";
    inline constexpr const char* tune       = "tune";
    inline constexpr const char* volume     = "volume";
    // voice input (setup, not part of presets)
    inline constexpr const char* voiceSource= "voiceSource";  // 0 = MIC (direct), 1 = DAW side chain
    inline constexpr const char* micGain    = "micGain";
    // auto-tune (sing without keys: the synth follows your pitch, snapped to the scale)
    inline constexpr const char* autoTune   = "autoTune";
    inline constexpr const char* key        = "key";
    inline constexpr const char* scale      = "scale";
    inline constexpr const char* retune     = "retune";
    inline constexpr const char* octave     = "octave";
}

inline juce::String midiNoteName (float m)
{
    if (m < 0) return "--";
    static const char* n[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int i = juce::roundToInt (m);
    return juce::String (n[((i % 12) + 12) % 12]) + juce::String (i / 12 - 1);
}

inline juce::String vowelName (float v)
{
    static const char* names[] = { "OO", "OH", "AH", "EH", "EE" };
    const int i = juce::jlimit (0, 4, juce::roundToInt (v));
    if (std::abs (v - (float) i) < 0.15f) return names[i];
    const int a = juce::jlimit (0, 3, (int) std::floor (v));
    return juce::String (names[a]) + ">" + names[a + 1];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto pct = [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };
    auto db  = [] (float v, int) { return String (v, 1) + " dB"; };
    (void) db;
    auto sec = [] (float v, int) { return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, 2) + " s"; };

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> r, float def,
                     std::function<String (float, int)> fmt)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                        AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto addB = [&] (const char* id, const char* name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    using namespace ParamID;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { synthModel, 1 }, "Synth", StringArray { "Mini", "DX" }, 0));
    addB (poly,       "Poly", false);
    addF (tone,       "Tone",      { 0.0f, 1.0f }, 0.72f, pct);
    addF (character,  "Character", { 0.0f, 1.0f }, 0.15f, pct);
    addF (envAmt,     "Contour",   { 0.0f, 1.0f }, 0.35f, pct);
    addF (detune,     "Detune",    { 0.0f, 1.0f }, 0.12f, pct);
    addF (sub,        "Sub",       { 0.0f, 1.0f }, 0.15f, pct);
    addF (glide,      "Glide",     { 0.0f, 0.6f, 0.0f, 0.5f }, 0.06f, sec);
    addF (vibrato,    "Vibrato",   { 0.0f, 1.0f }, 0.15f, pct);

    addF (attack,     "Attack",    { 0.001f, 1.5f, 0.0f, 0.35f }, 0.004f, sec);
    addF (decay,      "Decay",     { 0.02f, 3.0f, 0.0f, 0.4f }, 0.45f, sec);
    addF (sustain,    "Sustain",   { 0.0f, 1.0f }, 0.8f, pct);
    addF (release,    "Release",   { 0.01f, 3.0f, 0.0f, 0.4f }, 0.12f, sec);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { mouthMode, 1 }, "Mouth", StringArray { "Voice", "Vowel" }, 0));
    addF (sensitivity,"Sensitivity", { 0.0f, 1.0f }, 0.6f, pct);
    addF (response,   "Response",    { 0.0f, 1.0f }, 0.3f, pct);
    addF (brightness, "Brightness",  { 0.0f, 1.0f }, 0.9f, pct);
    addF (consonants, "Consonants",  { 0.0f, 1.0f }, 0.3f, pct);
    addF (vowel,      "Vowel",       { 0.0f, 4.0f }, 2.0f, [] (float v, int) { return vowelName (v); });
    addF (wah,        "Wah",         { 0.0f, 1.0f }, 0.5f, pct);
    addF (talkTime,   "Talk Time",   { 0.02f, 0.8f, 0.0f, 0.5f }, 0.18f, sec);

    addF (drive,      "Drive",       { 0.0f, 1.0f }, 0.3f, pct);
    addF (tube,       "Tube",        { 0.0f, 1.0f }, 0.35f, pct);
    addF (leak,       "Leak",        { 0.0f, 1.0f }, 0.0f, pct);

    addB (doublerOn,  "Doubler On", false);
    addF (doubler,    "Doubler",     { 0.0f, 1.0f }, 0.5f, pct);
    addB (roomOn,     "Room On", false);
    addF (roomMix,    "Room Mix",    { 0.0f, 1.0f }, 0.2f, pct);
    addF (roomSize,   "Room Size",   { 0.0f, 1.0f }, 0.4f, pct);
    addF (bendRange,  "Bend Range",  { 1.0f, 12.0f, 1.0f }, 2.0f, [] (float v, int) { return String (juce::roundToInt (v)) + " st"; });
    addF (tune,       "Tune",        { 430.0f, 450.0f, 0.01f }, 440.0f, [] (float v, int) { return "A = " + String (v, 1) + " Hz"; });
    addF (volume,     "Volume",      { 0.0f, 1.0f, 0.01f }, 0.5f, pct);   // output level, 50 % = -6 dB

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { voiceSource, 1 }, "Voice Input", StringArray { "Microphone", "Side Chain" }, 0));
    addF (micGain,    "Mic Gain",    { -12.0f, 40.0f, 0.1f }, 0.0f, db);

    addB (autoTune,   "Auto-Tune", true);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { key, 1 }, "Key",
                    StringArray { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { scale, 1 }, "Scale",
                    StringArray { "Chromatic", "Major", "Minor", "Major Pentatonic", "Minor Pentatonic", "Blues", "Dorian", "Mixolydian" }, 1));
    addF (retune,     "Retune Speed", { 0.0f, 1.0f }, 0.35f, [] (float v, int) {
        const float ms = 2.0f * std::pow (100.0f, v);
        return v < 0.05f ? String ("Robot") : String (juce::roundToInt (ms)) + " ms"; });
    addF (octave,     "Octave",       { -2.0f, 2.0f, 1.0f }, 0.0f, [] (float v, int) {
        const int o = juce::roundToInt (v); return o == 0 ? String ("0") : (o > 0 ? "+" : "") + String (o); });

    return layout;
}
