#pragma once

#include "hw.h"
#include "wreath.h"
#include "Utility/dsp.h"
#include <string>

namespace wreath
{
    using namespace daisy;
    using namespace daisysp;

    constexpr int kMaxPages{5};

    const char *pageNames[] = {
        "Wreath",
        "Speed",
        "Length",
        "Movement",
        "Mode",
    };
    int currentPage{0};
    bool pageSelected{false};

    enum class MenuClickOp
    {
        STOP,
        MENU,
        FREEZE,
        RESET,
        DUAL,
    };

    MenuClickOp clickOp{MenuClickOp::STOP};
    bool buttonPressed{false};

    const char *movementNames[] = {
        "Forward",
        "Backwards",
        "Pendulum",
        "Drunk",
        "Random",
    };

    const char *modeNames[] = {
        "Mimeo",
        "Cross",
        //"Mode 2",
        "Dual",
    };

    int currentLooper{0};

    void UpdateControls()
    {
        if (!looper.IsStartingUp())
        {
            ProcessControls();

            looper.SetDryWet(knob1Value);
            looper.SetFeedback(knob2Value);

            // Handle CV1 as trigger input for resetting the read position to
            // the loop start point.
            if (isCv1Connected)
            {
                looper.mustRestart = cv1Trigger;
            }

            // Handle CV2 as loop start point when frozen.
            if (looper.IsFrozen() && isCv2Connected)
            {
                looper.SetLoopStart(fmap(cv2.Value(), 0, looper.GetBufferSamples(0)));
            }
        }
    }

    void UpdateOled()
    {
        int width = hw.display.Width();

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
            float seconds = looper.GetBufferSeconds(currentLooper);
            float frac = seconds - (int)seconds;
            float inte = seconds - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + "/" + std::to_string(kBufferSeconds) + "s";
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

                float frac = position - (int)position;
                float inte = position - frac;
                // Write read position in seconds.
                if (loopLength > 1.f)
                {
                    str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
                }
                else
                {
                    position *= 1000;
                    str = std::to_string(static_cast<int>(position));
                }

                // Write the loop length in seconds.
                frac = loopLength - (int)loopLength;
                inte = loopLength - frac;
                if (loopLength > 1.f)
                {
                    str += "/" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + "s";
                }
                else
                {
                    loopLength *= 1000;
                    str += "/" + std::to_string(static_cast<int>(loopLength)) + "ms";
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
                hw.display.DrawRect(cursor, 23 + y, cursor, 24 + y, true, true);
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
            if (currentPage == 0)
            {
                // Page 0: Gain.
                str = "x" + std::to_string(static_cast<int>(looper.GetGain()));
            }
            if (currentPage == 1)
            {
                // Page 1: Speed.
                float frac = looper.GetSpeed(currentLooper) - (int)looper.GetSpeed(currentLooper);
                float inte = looper.GetSpeed(currentLooper) - frac;
                str = "x" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
            }
            else if (currentPage == 2)
            {
                // Page 2: Length.
                float loopLength = looper.GetLoopLengthSeconds(currentLooper);
                if (loopLength > 1.f)
                {
                    float frac = loopLength - (int)loopLength;
                    float inte = loopLength - frac;
                    str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + "s";
                }
                else
                {
                    loopLength *= 1000;
                    str = std::to_string(static_cast<int>(loopLength)) + "ms";
                }
                //str += "/" + std::to_string(loopers[0].GetLoopLength());
            }
            else if (currentPage == 3)
            {
                // Page 3: Movement.
                str = movementNames[static_cast<int>(looper.GetMovement(currentLooper))];
            }
            else if (currentPage == 4)
            {
                // Page 4: Mode.
                str = modeNames[static_cast<int>(looper.GetMode())];
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

    UiEventQueue eventQueue;

    void ProcessEvent(const UiEventQueue::Event& e)
    {
        switch(e.type)
        {
            case UiEventQueue::Event::EventType::buttonPressed:
                buttonPressed = false;
                break;

            case UiEventQueue::Event::EventType::buttonReleased:
                if (clickOp == MenuClickOp::STOP)
                {
                    // Stop buffering.
                    looper.mustStopBuffering = true;
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::FREEZE)
                {
                    // Toggle freeze.
                    looper.ToggleFreeze();
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::RESET)
                {
                    // ResetBuffer buffers.
                    looper.mustResetBuffer = true;
                    currentPage = 0;
                    pageSelected = false;
                    clickOp = MenuClickOp::STOP;
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
                        pageSelected = false;
                    }
                }
                break;

            case UiEventQueue::Event::EventType::encoderTurned:
                if (pageSelected && 0 != e.asEncoderTurned.increments)
                {
                    if (currentPage == 0)
                    {
                        // Page 0: Gain.
                        float gain = looper.GetGain();
                        gain += e.asEncoderTurned.increments;
                        looper.SetGain(fclamp(gain, 0, 10));
                    }
                    if (currentPage == 1)
                    {
                        // Page 1: Speed.
                        float steps{ e.asEncoderTurned.increments * 0.05f};
                        looper.IncrementSpeed(currentLooper, steps);
                        if (!looper.IsDualMode())
                        {
                            looper.IncrementSpeed(1, steps);
                        }
                    }
                    else if (currentPage == 2)
                    {
                        // Page 2: Length.
                        // TODO: micro-steps for v/oct.
                        int samples{};
                        samples = (looper.GetLoopLength(currentLooper) > 480) ? std::floor(looper.GetLoopLength(currentLooper) * 0.1f) : kMinSamples;
                        samples *=  e.asEncoderTurned.increments;
                        looper.IncrementLoopLength(currentLooper, samples);
                        if (!looper.IsDualMode())
                        {
                            looper.IncrementLoopLength(1, samples);
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
                else if (!buttonPressed)
                {
                    currentPage +=  e.asEncoderTurned.increments;
                    currentPage = fclamp(currentPage, 0, kMaxPages - 1);
                }
                break;

            default:
                break;
        }
    }

    void ProcessUi()
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

    void GenerateUiEvents()
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
            clickOp = MenuClickOp::RESET;
        }

        const auto increments = hw.encoder.Increment();
        if (increments != 0) {
            eventQueue.AddEncoderTurned(0, increments, 12);
        }
    }
}