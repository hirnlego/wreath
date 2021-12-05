#pragma once

#include "kxmx_bluemchen.h"

namespace wreath
{
    using namespace kxmx;
    using namespace daisy;

    constexpr size_t kSampleRate{48000};

    Bluemchen hw;

    Parameter knob1;
    Parameter knob2;
    Parameter knob1_dac;
    Parameter knob2_dac;
    Parameter cv1;
    Parameter cv2;

    void InitHw()
    {
        hw.Init();
        hw.SetAudioSampleRate(static_cast<daisy::SaiHandle::Config::SampleRate>(kSampleRate));
        hw.StartAdc();

        knob1.Init(hw.controls[hw.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
        knob2.Init(hw.controls[hw.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

        knob1_dac.Init(hw.controls[hw.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
        knob2_dac.Init(hw.controls[hw.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

        cv1.Init(hw.controls[hw.CTRL_3], 0.0f, 1.0f, Parameter::LINEAR);
        cv2.Init(hw.controls[hw.CTRL_4], 0.0f, 1.0f, Parameter::LINEAR);
    }
}