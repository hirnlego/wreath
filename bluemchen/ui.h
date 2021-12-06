#pragma once

#include <string>
#include "Utility/dsp.h"
#include "hw.h"
#include "wreath.h"

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
        "Mode 2",
        "Dual",
    };

    float cv1Value{};
    bool trigger{};
    bool raising{};

    int currentLooper{0};

    UiEventQueue eventQueue;

    void UpdateControls()
    {
        if (!loopers[currentLooper].IsStartingUp())
        {
            //hw.ProcessAllControls();

            knob1.Process();
            knob2.Process();

            hw.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, static_cast<uint16_t>(knob1_dac.Process()));
            hw.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, static_cast<uint16_t>(knob2_dac.Process()));

            cv1.Process();
            cv2.Process();

            // Handle dry/wet knob.
            if (std::abs(dryWet - knob1.Value()) > 0.01f)
            {
                dryWet = knob1.Value();
                loopers[0].SetDryWet(dryWet);
                loopers[1].SetDryWet(dryWet);
            }
            // Handle feedback/start knob.
            if (std::abs(feedback - knob2.Value()) > 0.01f)
            {
                feedback = knob2.Value();
                loopers[0].SetFeedback(feedback);
                loopers[1].SetFeedback(feedback);
            }

            // Handle trigger (restart) input.
            raising = cv1.Value() < cv1Value;
            if (!trigger && raising && cv1.Value() > 0.5f)
            {
                loopers[0].Restart();
                loopers[1].Restart();
                trigger = true;
            }
            else if (!raising && cv1.Value() < 0.5f)
            {
                trigger = false;
            }
            cv1Value = cv1.Value();
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

        if (loopers[currentLooper].IsStartingUp())
        {
            str = "Wait...";
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else if (loopers[currentLooper].IsBuffering())
        {
            // Buffering...
            str = "Enc stops";
            hw.display.SetCursor(0, 8);
            hw.display.WriteString(cstr, Font_6x8, true);
            str = "buffering";
            hw.display.SetCursor(0, 16);
            hw.display.WriteString(cstr, Font_6x8, true);
            // Write seconds buffered.
            float seconds = loopers[currentLooper].GetBufferSeconds();
            float frac = seconds - (int)seconds;
            float inte = seconds - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100)) + "/" + std::to_string(kBufferSeconds) + "s";
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else
        {
            step = width / (float)loopers[currentLooper].GetBufferSamples();

            if (!pageSelected)
            {
                float loopLength = loopers[currentLooper].GetLoopLengthSeconds();
                float position = loopers[currentLooper].GetPositionSeconds();

                float frac = position - (int)position;
                float inte = position - frac;
                // Write read position in seconds.
                if (loopLength > 1.f)
                {
                    str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100));
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
                    str += "/" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100)) + "s";
                }
                else
                {
                    loopLength *= 1000;
                    str += "/" + std::to_string(static_cast<int>(loopLength)) + "ms";
                }

                hw.display.SetCursor(0, 11);
                hw.display.WriteString(cstr, Font_6x8, true);
            }

            /*
            // Draw the loop bars.
            for (int i = 0; i < 2; i++)
            {
                int y{6 * i};
                int start = std::floor(loopers[i].GetLoopStart() * step);
                int end = std::floor(loopers[i].GetLoopEnd() * step);
                if (loopers[i].GetLoopStart() < loopers[i].GetLoopEnd())
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
                cursor = std::floor(loopers[i].GetPosition() * step);
                hw.display.DrawRect(cursor, 20 + y, cursor, 21 + y, true, true);
                // Draw the start position depending on the looper movement.
                if (Looper::Movement::FORWARD == loopers[i].GetMovement())
                {
                    cursor = start;
                }
                else if (Looper::Movement::BACKWARDS == loopers[i].GetMovement())
                {
                    cursor = end;
                }
                else if (Looper::Movement::PENDULUM == loopers[i].GetMovement())
                {
                    cursor = loopers[i].IsGoingForward() ? end : start;
                }
                else
                {
                    cursor = std::floor(loopers[i].GetNextPosition() * step);
                }
                hw.display.DrawRect(cursor, 23 + y, cursor, 24 + y, true, true);
            }
            */

            str = std::to_string(loopers[0].GetBufferSamples());
            hw.display.SetCursor(0, 16);
            hw.display.WriteString(cstr, Font_6x8, true);
            str = std::to_string(loopers[1].GetBufferSamples());
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }

        if (pageSelected)
        {
            if (currentPage == 1)
            {
                // Page 1: Speed.
                float frac = loopers[currentLooper].GetSpeed() - (int)loopers[currentLooper].GetSpeed();
                float inte = loopers[currentLooper].GetSpeed() - frac;
                str = "x" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100));
            }
            else if (currentPage == 2)
            {
                // Page 2: Length.
                float loopLength = loopers[currentLooper].GetLoopLengthSeconds();
                if (loopLength > 1.f)
                {
                    float frac = loopLength - (int)loopLength;
                    float inte = loopLength - frac;
                    str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100)) + "s";
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
                str = movementNames[static_cast<int>(loopers[currentLooper].GetMovement())];
            }
            else if (currentPage == 4)
            {
                // Page 4: Mode.
                str = modeNames[static_cast<int>(loopers[currentLooper].GetMode())];
            }

            hw.display.SetCursor(0, 11);
            hw.display.WriteString(cstr, Font_6x8, true);
        }

        if (loopers[0].IsFrozen())
        {
            str = "*";
            hw.display.SetCursor(width - 6, 0);
            hw.display.WriteString(cstr, Font_6x8, true);
        }

        hw.display.Update();
    }

    void UpdateMenu()
    {
        if (!loopers[0].IsBuffering())
        {
            if (pageSelected)
            {
                if (currentPage == 1)
                {
                    // Page 1: Speed.
                    if (hw.encoder.Increment() > 0)
                    {
                        loopers[currentLooper].SetSpeed(loopers[currentLooper].GetSpeed() + 0.05f);
                        if (!loopers[currentLooper].IsDualMode())
                        {
                            loopers[1].SetSpeed(loopers[currentLooper].GetSpeed());
                        }
                    }
                    else if (hw.encoder.Increment() < 0)
                    {
                        loopers[currentLooper].SetSpeed(loopers[currentLooper].GetSpeed() - 0.05f);
                        if (!loopers[currentLooper].IsDualMode())
                        {
                            loopers[1].SetSpeed(loopers[currentLooper].GetSpeed());
                        }
                    }
                }
                else if (currentPage == 2)
                {
                    // Page 2: Length.
                    // TODO: micro-steps for v/oct.
                    int samples{};
                    samples = (loopers[currentLooper].GetLoopLength() > 480) ? std::floor(loopers[currentLooper].GetLoopLength() * 0.1) : kMinSamples;
                    if (hw.encoder.Increment() > 0)
                    {
                        loopers[currentLooper].IncrementLoopLength(samples);
                        if (!loopers[currentLooper].IsDualMode())
                        {
                            loopers[1].IncrementLoopLength(samples);
                        }
                    }
                    else if (hw.encoder.Increment() < 0)
                    {
                        loopers[currentLooper].DecrementLoopLength(samples);
                        if (!loopers[currentLooper].IsDualMode())
                        {
                            loopers[1].DecrementLoopLength(samples);
                        }
                    }
                }
                else if (currentPage == 3)
                {
                    // Page 3: Movement.
                    int currentMovement{loopers[currentLooper].GetMovement()};
                    currentMovement += hw.encoder.Increment();
                    loopers[currentLooper].SetMovement(static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                    if (!loopers[currentLooper].IsDualMode())
                    {
                        loopers[1].SetMovement(static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                    }
                }
                else if (currentPage == 4)
                {
                    // Page 4: Mode.
                    bool isDualMode{loopers[0].IsDualMode()};
                    for (int i = 0; i < 2; i++)
                    {
                        int currentMode{loopers[i].GetMode()};
                        currentMode += hw.encoder.Increment();
                        loopers[i].SetMode(static_cast<Looper::Mode>(fclamp(currentMode, 0, Looper::Mode::LAST_MODE - 1)));
                    }
                    // When switching from dual mode to a coupled mode,
                    // reset the loopers.
                    if (isDualMode && !loopers[0].IsDualMode())
                    {
                        loopers[0].SetMovement(Looper::Movement::FORWARD);
                        loopers[1].SetMovement(Looper::Movement::FORWARD);
                        loopers[0].SetSpeed(1.0f);
                        loopers[1].SetSpeed(1.0f);
                        loopers[0].ResetLoopLength();
                        loopers[1].ResetLoopLength();
                        loopers[0].Restart();
                        loopers[1].Restart();
                        currentLooper = 0;
                    }
                }
            }
            else if (!buttonPressed)
            {
                currentPage += hw.encoder.Increment();
                if (currentPage < 0)
                {
                    currentPage = 0;
                }
                else if (currentPage > kMaxPages - 1)
                {
                    currentPage = kMaxPages - 1;
                }
            }

            if (hw.encoder.TimeHeldMs() >= 500.f)
            {
                clickOp = MenuClickOp::FREEZE;
            }
            if (hw.encoder.TimeHeldMs() >= 2000.f)
            {
                clickOp = MenuClickOp::RESET;
            }
        }

        if (hw.encoder.RisingEdge())
        {
            buttonPressed = true;
        }
        if (hw.encoder.FallingEdge())
        {
            if (clickOp == MenuClickOp::STOP)
            {
                // Stop buffering.
                //loopers[0].StopBuffering();
                //loopers[1].StopBuffering();
                mustStopBuffering = true;
                clickOp = MenuClickOp::MENU;
            }
            else if (clickOp == MenuClickOp::FREEZE)
            {
                // Toggle freeze.
                loopers[0].ToggleFreeze();
                loopers[1].ToggleFreeze();
                clickOp = MenuClickOp::MENU;
            }
            else if (clickOp == MenuClickOp::RESET)
            {
                // ResetBuffer buffers.
                loopers[0].ResetBuffer();
                loopers[1].ResetBuffer();
                currentPage = 0;
                pageSelected = false;
                clickOp = MenuClickOp::STOP;
            }
            else if (clickOp == MenuClickOp::DUAL)
            {
                currentLooper = 1; // Select the second looper
                clickOp = MenuClickOp::MENU;
            }
            else if (currentPage != 0)
            {
                // When not in the main page, toggle the page selection.
                if (!pageSelected)
                {
                    pageSelected = true;
                    if (loopers[0].IsDualMode())
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

            buttonPressed = false;
        }
    }

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
                    //loopers[0].StopBuffering();
                    //loopers[1].StopBuffering();
                    mustStopBuffering = true;
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::FREEZE)
                {
                    // Toggle freeze.
                    loopers[0].ToggleFreeze();
                    loopers[1].ToggleFreeze();
                    clickOp = MenuClickOp::MENU;
                }
                else if (clickOp == MenuClickOp::RESET)
                {
                    // ResetBuffer buffers.
                    loopers[0].ResetBuffer();
                    loopers[1].ResetBuffer();
                    currentPage = 0;
                    pageSelected = false;
                    clickOp = MenuClickOp::STOP;
                }
                else if (clickOp == MenuClickOp::DUAL)
                {
                    currentLooper = 1; // Select the second looper
                    clickOp = MenuClickOp::MENU;
                }
                else if (currentPage != 0)
                {
                    // When not in the main page, toggle the page selection.
                    if (!pageSelected)
                    {
                        pageSelected = true;
                        if (loopers[0].IsDualMode())
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

                break;
        }
    }

    void ProcessUi()
    {
        while (!eventQueue.IsQueueEmpty())
        {
            UiEventQueue::Event e = eventQueue.GetAndRemoveNextEvent();
            if (e.type != UiEventQueue::Event::EventType::invalid)
            {
                ProcessEvent(e);
            }
        }
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