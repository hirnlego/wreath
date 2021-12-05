#pragma once

#include "looper.h"

namespace wreath
{

    constexpr int kBufferSeconds{150}; // 2:30 minutes max

    const size_t kBufferSamples{kSampleRate * kBufferSeconds};

    float DSY_SDRAM_BSS buffer_l[kBufferSamples];
    float DSY_SDRAM_BSS buffer_r[kBufferSamples];

    float dryWet{};
    float feedback{};

    Looper loopers[2];

    bool mustStopBuffering{};
    bool mustResetBuffer{};

    void InitLoopers()
    {
        loopers[0].Init(kSampleRate, buffer_l, kBufferSeconds);
        loopers[1].Init(kSampleRate, buffer_r, kBufferSeconds);
    }

}