#pragma once

#include "kxmx_bluemchen.h"

namespace wreath
{
    using namespace kxmx;
    using namespace daisy;

    // The minimum difference in parameter value to be registered.
    constexpr float kMinValueDelta{0.01f};
    // The trigger threshold value.
    constexpr float kTriggerThres{0.5f};
    // Maximum BPM supported.
    constexpr int kMaxBpm{300};

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

    static size_t begin{};
    static size_t end{};

    size_t ms{};
    size_t cv1Bpm{};

    inline static int CalculateBpm()
    {
        end = ms;
        // Handle the ms reset.
        if (end < begin) {
            end += 10000;
        }

        return std::round((1000.f / (end - begin)) * 60);
    }

    inline void InitHw(float knobSlewSeconds, float cvSlewSeconds)
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

    inline void UpdateClock()
    {
        if (ms > 10000)
        {
            ms = 0;
        }
        ms++;
    }

    inline void ProcessControls()
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

        isCv1Connected = std::abs(cv1Value - cv1.Value()) > kMinValueDelta;
        if (isCv1Connected) {
            cv1Trigger = false;
            raising = cv1.Value() < cv1Value;
            if (!triggered && raising && cv1.Value() >= kTriggerThres)
            {
                int bpm = CalculateBpm();
                if (bpm < kMaxBpm) {
                    cv1Bpm = bpm;
                }
                triggered = true;
                cv1Trigger = true;
                begin = ms;
            }
            else if (!raising || cv1.Value() < kTriggerThres)
            {
                triggered = false;
            }
        }
        cv1Value = cv1.Value();

        isCv2Connected = std::abs(cv2Value - cv2.Value()) > kMinValueDelta;
        cv2Value = cv2.Value();
    }
}