#pragma once
#include "Parameters.h"
#include <vector>
#include <utility>

struct FactoryPreset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values; // plain (real-world) values; unlisted params use defaults
};

inline const std::vector<FactoryPreset>& getFactoryPresets()
{
    using namespace ParamID;
    static const std::vector<FactoryPreset> presets = {
        { "Dayton Funk Lead",     { { synthModel, 0 }, { tone, 0.74f }, { glide, 0.06f }, { vibrato, 0.2f },
                                    { drive, 0.35f }, { doublerOn, 1 }, { doubler, 0.4f } } },
        { "DX Talker",            { { synthModel, 1 }, { tone, 0.62f }, { character, 0.55f }, { envAmt, 0.25f },
                                    { detune, 0.1f }, { drive, 0.3f }, { doublerOn, 1 }, { doubler, 0.35f } } },
        { "Smooth Ballad Talk",   { { synthModel, 0 }, { tone, 0.6f }, { glide, 0.12f }, { vibrato, 0.35f },
                                    { response, 0.5f }, { attack, 0.02f }, { release, 0.3f }, { doublerOn, 1 },
                                    { roomOn, 1 }, { roomMix, 0.25f }, { roomSize, 0.55f } } },
        { "West Coast Lead",      { { synthModel, 1 }, { tone, 0.7f }, { character, 0.7f }, { sub, 0.35f },
                                    { glide, 0.08f }, { drive, 0.45f }, { doublerOn, 1 }, { doubler, 0.55f },
                                    { roomOn, 1 }, { roomMix, 0.15f } } },
        { "Vowel Wah (No Mic)",   { { mouthMode, 1 }, { vowel, 1.2f }, { wah, 0.8f }, { talkTime, 0.22f },
                                    { glide, 0.05f }, { doublerOn, 1 }, { doubler, 0.4f } } },
        { "Vowel Robot Chords",   { { mouthMode, 1 }, { poly, 1 }, { vowel, 2.5f }, { wah, 0.3f }, { synthModel, 1 },
                                    { character, 0.45f }, { attack, 0.01f }, { release, 0.25f },
                                    { roomOn, 1 }, { roomMix, 0.25f } } },
        { "Poly Talk Pads",       { { poly, 1 }, { attack, 0.05f }, { release, 0.4f }, { tone, 0.66f },
                                    { detune, 0.3f }, { doublerOn, 1 }, { roomOn, 1 }, { roomMix, 0.3f } } },
        { "Dirty Driver",         { { synthModel, 0 }, { drive, 0.85f }, { tube, 0.6f }, { character, 0.35f },
                                    { envAmt, 0.5f }, { consonants, 0.4f } } },
        { "Dry Studio",           { { tube, 0.2f }, { drive, 0.2f } } },
    };
    return presets;
}
