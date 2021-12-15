#pragma once

#include "hw.h"
#include "wreath.h"
#include "Utility/dsp.h"
#include <string>

namespace wreath
{
    using namespace daisy;
    using namespace daisysp;

    namespace mpaland {
        #include "printf.h"
        #include "printf.c"
    };

    constexpr int kMaxPages{5};
    const char *pageNames[] = {
        "Wreath",
        "Speed",
        "Length",
        "Movement",
        "Mode",
    };
    const char *movementNames[] = {
        "Forward",
        "Back",
        "Pendulum",
        "Drunk",
    };
    const char *modeNames[] = {
        "Mono",
        "Cross",
        "Dual",
    };

    enum class MenuClickOp
    {
        MENU,
        FREEZE,
        CLEAR,
        RESET,
        DUAL,
    };

    int currentPage{0};
    bool pageSelected{false};
    MenuClickOp clickOp{MenuClickOp::MENU};
    bool buttonPressed{false};
    int currentLooper{0};
    UiEventQueue eventQueue;

    inline static void UpdateOled()
    {
        int width{hw.display.Width()};

        hw.display.Fill(false);

        std::string str = pageNames[currentPage];
        char *cstr = &str[0];
        hw.display.SetCursor(0, 0);
        hw.display.WriteString(cstr, Font_6x8, !pageSelected);

        float step = width;

        if (looper.IsStartingUp())
        {
            str = "Wait...";
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else if (looper.IsBuffering())
        {
            // Buffering...
            str = "Enc stops";
            hw.display.SetCursor(0, 8);
            hw.display.WriteString(cstr, Font_6x8, true);
            str = "buffering";
            hw.display.SetCursor(0, 16);
            hw.display.WriteString(cstr, Font_6x8, true);
            // Write seconds buffered.
            mpaland::sprintf(cstr, "%.2f%s", looper.GetBufferSeconds(currentLooper), ("/" + std::to_string(kBufferSeconds) + "s").c_str());
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else
        {
            step = width / (float)looper.GetBufferSamples(currentLooper);

            if (!pageSelected)
            {
                float loopLength = looper.GetLoopLengthSeconds(currentLooper);
                float position = looper.GetPositionSeconds(currentLooper);
                // Write read position and the loop length in seconds.
                if (loopLength > 1.f)
                {
                    mpaland::sprintf(cstr, "%.2f/%.2fs", position, loopLength);
                }
                else
                {
                    mpaland::sprintf(cstr, "%.0f/%.0fms", position * 1000, loopLength * 1000);
                }

                hw.display.SetCursor(0, 11);
                hw.display.WriteString(cstr, Font_6x8, true);
            }

            // Draw the loop bars.
            for (int i = 0; i < 2; i++)
            {
                int y{6 * i};
                int start = std::floor(looper.GetLoopStart(i) * step);
                int end = std::floor(looper.GetLoopEnd(i) * step);
                if (looper.GetLoopStart(i) < looper.GetLoopEnd(i))
                {
                    // Normal loop (start position before end position).
                    hw.display.DrawRect(start, 22 + y, end, 22 + y, true, true);
                }
                else
                {
                    // Inverse loop (end position before start position).
                    hw.display.DrawRect(0, 22 + y, end, 22 + y, true, true);
                    hw.display.DrawRect(start, 22 + y, width, 22 + y, true, true);
                }
                int cursor{};
                // Draw the read position.
                cursor = std::floor(looper.GetReadPos(i) * step);
                hw.display.DrawRect(cursor, 20 + y, cursor, 21 + y, true, true);
                // Draw the start position depending on the looper movement.
                if (Looper::Movement::FORWARD == looper.GetMovement(i))
                {
                    cursor = start;
                }
                else if (Looper::Movement::BACKWARDS == looper.GetMovement(i))
                {
                    cursor = end;
                }
                else if (Looper::Movement::PENDULUM == looper.GetMovement(i))
                {
                    cursor = looper.IsGoingForward(i) ? end : start;
                }
                else
                {
                    cursor = std::floor(looper.GetNextReadPos(i) * step);
                }
                hw.display.DrawRect(cursor, 21 + y, cursor, 21 + y, true, true);
                // Draw the write position.
                cursor = std::floor(looper.GetWritePos(i) * step);
                hw.display.DrawRect(cursor, 23 + y, cursor, 23 + y, true, true);

                cursor = std::floor(looper.temp() * step);
                hw.display.DrawRect(cursor, 24 + y, cursor, 24 + y, true, true);
            }

            /*
            str = std::to_string(looper.GetBufferSamples(0));
            hw.display.SetCursor(0, 16);
            hw.display.WriteString(cstr, Font_6x8, true);
            str = std::to_string(looper.GetBufferSamples(1));
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
            */
        }

        if (pageSelected)
        {
            const std::string sLR[3]{"", "L|", "R|"};
            const char *cLR = sLR[looper.IsDualMode() * (currentLooper + 1)].c_str();
            if (currentPage == 0)
            {
                // Page 0: Gain.
                mpaland::sprintf(cstr, "x%.2f", looper.GetGain());
            }
            if (currentPage == 1)
            {
                // Page 1: Speed.
                mpaland::sprintf(cstr, "%s%.2fx", cLR, looper.GetSpeedMult(currentLooper));
            }
            else if (currentPage == 2)
            {
                // Page 2: Length.
                float loopLength = looper.GetLoopLengthSeconds(currentLooper);
                if (loopLength > 1.f)
                {
                    mpaland::sprintf(cstr, "%s%.2fs", cLR, loopLength);
                }
                else
                {
                    mpaland::sprintf(cstr, "%s%.0fms", cLR, loopLength * 1000);
                }
            }
            else if (currentPage == 3)
            {
                // Page 3: Movement.
                mpaland::sprintf(cstr, "%s%s", cLR, movementNames[static_cast<int>(looper.GetMovement(currentLooper))]);
            }
            else if (currentPage == 4)
            {
                // Page 4: Mode.
                mpaland::sprintf(cstr, "%s", modeNames[static_cast<int>(looper.GetMode())]);
            }

            hw.display.SetCursor(0, 11);
            hw.display.WriteString(cstr, Font_6x8, true);
        }

        if (looper.IsFrozen())
        {
            str = "*";
            hw.display.SetCursor(width - 6, 0);
            hw.display.WriteString(cstr, Font_6x8, true);
        }

        hw.display.Update();
    }

    inline static void ProcessEvent(const UiEventQueue::Event& e)
    {
        switch(e.type)
        {
            case UiEventQueue::Event::EventType::buttonPressed:
                buttonPressed = false;
                break;

            case UiEventQueue::Event::EventType::buttonReleased:
                if (looper.IsBuffering())
                {
                    // Stop buffering.
                    looper.mustStopBuffering = true;
                }
                else if (clickOp == MenuClickOp::FREEZE)
                {
                    // Toggle freeze.
                    looper.ToggleFreeze();
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::CLEAR)
                {
                    // Clear the buffer.
                    looper.mustClearBuffer = true;
                    currentPage = 0;
                    pageSelected = false;
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::RESET)
                {
                    // Reset the looper.
                    looper.mustResetLooper = true;
                    currentPage = 0;
                    pageSelected = false;
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::DUAL)
                {
                    currentLooper = 1; // Select the second looper
                    clickOp = MenuClickOp::MENU;
                }
                else
                {
                    // When not in the main page, toggle the page selection.
                    if (!pageSelected)
                    {
                        pageSelected = true;
                        if (looper.IsDualMode())
                        {
                            // In dual mode the controls for the two loopers are
                            // independent.
                            currentLooper = 0; // Select the first looper
                            clickOp = MenuClickOp::DUAL;
                        }
                    }
                    else {
                        currentLooper = 0; // Select the first looper
                        pageSelected = false;
                    }
                }
                break;

            case UiEventQueue::Event::EventType::encoderTurned:
                if (pageSelected)
                {
                    if (currentPage == 0)
                    {
                        // Page 0: Gain.
                        float gain = looper.GetGain();
                        float steps{static_cast<float>(e.asEncoderTurned.increments) * 0.1f};
                        looper.SetGain(fclamp(gain + steps, 0.1f, 10.f));
                    }
                    if (currentPage == 1)
                    {
                        // Page 1: Speed.
                        float currentSpeedMult{looper.GetSpeedMult(currentLooper)};
                        float steps{static_cast<float>(e.asEncoderTurned.increments)};
                        steps *= ((currentSpeedMult < 5.f) || (steps < 0 && currentSpeedMult - 5.f <= kMinSpeedMult)) ? kMinSpeedMult : 5.f;
                        currentSpeedMult = fclamp(currentSpeedMult + steps, kMinSpeedMult, kMaxSpeedMult);
                        looper.SetSpeedMult(currentLooper, currentSpeedMult);
                        if (!looper.IsDualMode())
                        {
                            looper.SetSpeedMult(1, currentSpeedMult);
                        }
                    }
                    else if (currentPage == 2)
                    {
                        // Page 2: Length.
                        // TODO: micro-steps for v/oct.
                        size_t currentLoopLength{looper.GetLoopLength(currentLooper)};
                        int samples{e.asEncoderTurned.increments};
                        samples *= (currentLoopLength >= kMinSamplesForTone) ? std::floor(currentLoopLength * 0.1f) : kMinLoopLengthSamples;
                        looper.SetLoopLength(currentLooper, currentLoopLength + samples);
                        if (!looper.IsDualMode())
                        {
                            looper.SetLoopLength(1, currentLoopLength + samples);
                        }
                    }
                    else if (currentPage == 3)
                    {
                        // Page 3: Movement.
                        int currentMovement{looper.GetMovement(currentLooper) + e.asEncoderTurned.increments};
                        looper.SetMovement(currentLooper, static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                        if (!looper.IsDualMode())
                        {
                            looper.SetMovement(1, static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                        }
                    }
                    else if (currentPage == 4)
                    {
                        // Page 4: Mode.
                        bool isDualMode{looper.IsDualMode()};
                        int currentMode{looper.GetMode() + e.asEncoderTurned.increments};
                        looper.SetMode(static_cast<StereoLooper::Mode>(fclamp(currentMode, 0, StereoLooper::Mode::LAST_MODE - 1)));
                        if (isDualMode && !looper.IsDualMode())
                        {
                            currentLooper = 0;
                        }
                    }
                }
                else if (!buttonPressed && !looper.IsBuffering())
                {
                    currentPage += e.asEncoderTurned.increments;
                    currentPage = fclamp(currentPage, 0, kMaxPages - 1);
                }
                break;

            case UiEventQueue::Event::EventType::encoderActivityChanged:
                looper.SetReading(!static_cast<bool>(e.asEncoderActivityChanged.newActivityType));
                break;

            default:
                break;
        }
    }

    void UpdateControls()
    {
        ProcessControls();

        if (knob1Changed)
        {
            looper.SetDryWet(knob1Value);
        }

        if (!looper.IsStartingUp())
        {
            looper.SetReading(!knob1Changed && !knob2Changed);

            if (knob2Changed)
            {
                if (looper.IsFrozen())
                {
                    static bool feedbackPickup{};

                    size_t leftStart{static_cast<size_t>(std::floor(knob2Value * looper.GetBufferSamples(0)))};
                    size_t rightStart{static_cast<size_t>(std::floor(knob2Value * looper.GetBufferSamples(1)))};

                    if (std::abs(static_cast<int>(leftStart - looper.GetLoopStart(0))) < static_cast<int>(looper.GetBufferSamples(0) * 0.1f) && !feedbackPickup)
                    {
                        feedbackPickup = true;
                    }
                    if (feedbackPickup)
                    {
                        looper.SetLoopStart(0, leftStart);
                        looper.SetLoopStart(1, rightStart);
                    }
                }
                else
                {
                    looper.SetFeedback(knob2Value);
                }
            }

            // Handle CV1 as trigger input for resetting the read position to
            // the loop start point.
            if (isCv1Connected)
            {
                looper.mustRestart = cv1Trigger;
            }

            // Handle CV2 as loop start point when frozen.
            if (looper.IsFrozen() && isCv2Connected)
            {
                //looper.SetLoopStart(fmap(cv2.Value(), 0, looper.GetBufferSamples(0)));
            }
        }
    }

    inline void GenerateUiEvents()
    {
        if (hw.encoder.RisingEdge()) {
            eventQueue.AddButtonPressed(0, 1);
        }

        if (hw.encoder.FallingEdge()) {
            eventQueue.AddButtonReleased(0);
        }

        if (hw.encoder.TimeHeldMs() >= 500.f)
        {
            clickOp = MenuClickOp::FREEZE;
        }
        if (hw.encoder.TimeHeldMs() >= 2000.f)
        {
            clickOp = MenuClickOp::CLEAR;
        }
        if (hw.encoder.TimeHeldMs() >= 5000.f)
        {
            clickOp = MenuClickOp::RESET;
        }

        const auto increments = hw.encoder.Increment();
        static bool active = false;
        if (increments != 0)
        {
            active = true;
            eventQueue.AddEncoderTurned(0, increments, 12);
            eventQueue.AddEncoderActivityChanged(0, active);
        }
        else if (active)
        {
            active = false;
            eventQueue.AddEncoderActivityChanged(0, active);
        }
    }

    inline void ProcessUi()
    {
        UpdateControls();

        while (!eventQueue.IsQueueEmpty())
        {
            UiEventQueue::Event e = eventQueue.GetAndRemoveNextEvent();
            if (e.type != UiEventQueue::Event::EventType::invalid)
            {
                ProcessEvent(e);
            }
        }

        UpdateOled();
    }
}