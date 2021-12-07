#pragma once

#include "kxmx_bluemchen.h"
#include <chrono>

namespace wreath
{
    using namespace kxmx;
    using namespace daisy;

    Bluemchen hw;

    Parameter knob1;
    Parameter knob2;
    Parameter knob1_dac;
    Parameter knob2_dac;
    Parameter cv1;
    Parameter cv2;

    float knob1Value{};
    float knob2Value{};
    float cv1Value{};
    float cv2Value{};
    bool cv1Trigger{};
    bool raising{};
    bool triggered{};
    bool isCv1Connected{};
    bool isCv2Connected{};
    int64_t cv1Clock{};

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

    void InitHw(float knobSlewSeconds, float cvSlewSeconds)
    {
        hw.Init();
        hw.StartAdc();

        knob1.Init(hw.controls[hw.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
        knob2.Init(hw.controls[hw.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

        knob1_dac.Init(hw.controls[hw.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
        knob2_dac.Init(hw.controls[hw.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

        cv1.Init(hw.controls[hw.CTRL_3], 0.0f, 1.0f, Parameter::LINEAR);
        cv2.Init(hw.controls[hw.CTRL_4], 0.0f, 1.0f, Parameter::LINEAR);

        hw.controls[hw.CTRL_1].SetCoeff(1.0f / (knobSlewSeconds * hw.AudioSampleRate() * 0.5f));
        hw.controls[hw.CTRL_2].SetCoeff(1.0f / (knobSlewSeconds * hw.AudioSampleRate() * 0.5f));
        hw.controls[hw.CTRL_3].SetCoeff(1.0f / (cvSlewSeconds * hw.AudioSampleRate() * 0.5f));
        hw.controls[hw.CTRL_4].SetCoeff(1.0f / (cvSlewSeconds * hw.AudioSampleRate() * 0.5f));
    }

    void ProcessControls()
    {
        hw.ProcessAllControls();

        knob1.Process();
        knob2.Process();

        hw.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, static_cast<uint16_t>(knob1_dac.Process()));
        hw.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, static_cast<uint16_t>(knob2_dac.Process()));

        cv1.Process();
        cv2.Process();

        knob1Value = knob1.Value();
        knob2Value = knob2.Value();

        isCv1Connected = std::abs(cv1Value - cv1.Value()) > 0.01f;
        if (isCv1Connected) {
            cv1Trigger = false;
            raising = cv1.Value() < cv1Value;
            if (!triggered && raising && cv1.Value() >= 0.3f)
            {
                end = std::chrono::steady_clock::now();
                double seconds = std::ceil(std::chrono::duration_cast<std::chrono::minutes>(end - begin).count() / 1000000.0) * 60.0;
                cv1Clock = static_cast<int64_t>(seconds);
                begin = std::chrono::steady_clock::now();
                triggered = true;
                cv1Trigger = true;
            }
            else if (!raising || cv1.Value() < 0.3f)
            {
                triggered = false;
            }
        }
        cv1Value = cv1.Value();

        isCv2Connected = std::abs(cv2Value - cv2.Value()) > 0.01f;
        cv2Value = cv2.Value();
    }
}