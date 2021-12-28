#pragma once

#include "hw.h"
#include "head.h"
#include "wreath.h"
#include "Utility/dsp.h"
#include <string>

namespace wreath
{
    using namespace daisy;
    using namespace daisysp;

    namespace mpaland
    {
#include "printf.h"
#include "printf.c"
    };

    const char *pageNames[] = {
        "Wreath",
        "Mix",
        "Feedback",
        "Filter",
        "Speed",
        "Start",
        "Length",
        "Movement",
        "Mode",
        "Gain",
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

    enum Page
    {
        HOME,
        MIX,
        FEEDBACK,
        FILTER,
        SPEED,
        START,
        LENGTH,
        MOVEMENT,
        MODE,
        GAIN,
        LAST_PAGE,
    };

    enum class MenuClickOp
    {
        MENU,
        FREEZE,
        CLEAR,
        RESET,
        DUAL,
    };

    short currentMovement{};
    Page selectedPage{Page::HOME};
    bool enteredPage{false};
    MenuClickOp clickOp{MenuClickOp::MENU};
    bool buttonPressed{false};
    short currentLooper{0};
    UiEventQueue eventQueue;

    static short MovementToMenu(short channel)
    {
        short movement{looper.GetMovement(channel)};

        return movement == 0 ? movement + !looper.IsGoingForward(channel) : movement + 1;
    }

    static void MenuToMovement(short movement)
    {
        Movement m = (movement < 2) ? Movement::NORMAL : static_cast<Movement>(movement - 1);
        looper.SetMovement(looper.IsDualMode() ? currentLooper : StereoLooper::BOTH, m);
        if (movement < 2)
        {
            looper.SetDirection(looper.IsDualMode() ? currentLooper : StereoLooper::BOTH, static_cast<Direction>(movement * -2 + 1));
        }
    }

    inline static void UpdateOled()
    {
        int width{hw.display.Width()};

        hw.display.Fill(false);

        std::string str = pageNames[selectedPage];
        char *cstr = &str[0];
        hw.display.SetCursor(0, 0);
        hw.display.WriteString(cstr, Font_6x8, !enteredPage);

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

            if (!enteredPage)
            {
                float loopLength = looper.GetLoopLengthSeconds(currentLooper);
                float position = looper.GetReadPosSeconds(currentLooper);
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
                if (Movement::NORMAL == looper.GetMovement(i) && looper.IsGoingForward(i))
                {
                    cursor = start;
                }
                else if (Movement::NORMAL == looper.GetMovement(i) && !looper.IsGoingForward(i))
                {
                    cursor = end;
                }
                else if (Movement::PENDULUM == looper.GetMovement(i))
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

        if (enteredPage)
        {
            const std::string sLR[3]{"", "L|", "R|"};
            const char *cLR = sLR[looper.IsDualMode() * (currentLooper + 1)].c_str();
            switch (selectedPage)
            {
            case Page::HOME:
                break;
            case Page::MIX:
                mpaland::sprintf(cstr, "%.2f", looper.GetMix());
                break;
            case Page::FEEDBACK:
                mpaland::sprintf(cstr, "%.2f", looper.GetFeedBack());
                break;
            case Page::FILTER:
                mpaland::sprintf(cstr, "%.0f", looper.GetFilter());
                break;
            case Page::GAIN:
                mpaland::sprintf(cstr, "x%.2f", looper.GetGain());
                break;
            case Page::SPEED:
                mpaland::sprintf(cstr, "%s%.2fx", cLR, looper.GetRate(currentLooper));
                break;
            case Page::START:
            {
                float loopStart = looper.GetLoopStartSeconds(currentLooper);
                if (loopStart > 1.f)
                {
                    mpaland::sprintf(cstr, "%s%.2fs", cLR, loopStart);
                }
                else
                {
                    mpaland::sprintf(cstr, "%s%.0fms", cLR, loopStart * 1000);
                }
                break;
            }
            case Page::LENGTH:
            {
                float loopLength = looper.GetLoopLengthSeconds(currentLooper);
                if (loopLength > 1.f)
                {
                    mpaland::sprintf(cstr, "%s%.2fs", cLR, loopLength);
                }
                else
                {
                    mpaland::sprintf(cstr, "%s%.0fms", cLR, loopLength * 1000);
                }
                break;
            }
            case Page::MOVEMENT:
                mpaland::sprintf(cstr, "%s%s", cLR, movementNames[MovementToMenu(currentLooper)]);
                break;
            case Page::MODE:
                mpaland::sprintf(cstr, "%s", modeNames[static_cast<int>(looper.GetMode())]);
                break;
            default:
                break;
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

    inline static void ProcessEvent(const UiEventQueue::Event &e)
    {
        switch (e.type)
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
                // Unfreeze if frozen.
                if (looper.IsFrozen())
                {
                    looper.ToggleFreeze();
                }
                // Clear the buffer.
                looper.mustClearBuffer = true;
                clickOp = MenuClickOp::MENU;
            }
            else if (clickOp == MenuClickOp::RESET)
            {
                // Reset the looper.
                looper.mustResetLooper = true;
                selectedPage = Page::HOME;
                enteredPage = false;
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
                if (!enteredPage)
                {
                    enteredPage = true;
                    if (looper.IsDualMode())
                    {
                        // In dual mode the controls for the two loopers are
                        // independent.
                        currentLooper = 0; // Select the first looper
                        clickOp = MenuClickOp::DUAL;
                    }
                }
                else
                {
                    currentLooper = 0; // Select the first looper
                    enteredPage = false;
                }
            }
            break;

        case UiEventQueue::Event::EventType::encoderTurned:
            if (enteredPage)
            {
                switch (selectedPage)
                {
                case Page::MIX:
                {
                    float steps{static_cast<float>(e.asEncoderTurned.increments) * 0.05f};
                    looper.nextMix = fclamp(looper.GetMix() + steps, 0.f, 2.f);
                    break;
                }
                case Page::FEEDBACK:
                {
                    float steps{static_cast<float>(e.asEncoderTurned.increments) * 0.01f};
                    looper.nextFeedback = fclamp(looper.GetFeedBack() + steps, 0.f, 1.f);
                    break;
                }
                case Page::FILTER:
                {
                    float steps{static_cast<float>(e.asEncoderTurned.increments) * 20.f};
                    looper.nextFilterValue = fclamp(looper.GetFilter() + steps, 0.f, 1000.f);
                    break;
                }
                case Page::GAIN:
                {
                    float steps{static_cast<float>(e.asEncoderTurned.increments) * 0.1f};
                    looper.nextGain = fclamp(looper.GetGain() + steps, 0.1f, 10.f);
                    break;
                }
                case Page::SPEED:
                {
                    float currentSpeedMult{looper.GetRate(currentLooper)};
                    float steps{static_cast<float>(e.asEncoderTurned.increments)};
                    steps *= ((currentSpeedMult < 5.f) || (steps < 0 && currentSpeedMult - 5.f <= kMinSpeedMult)) ? kMinSpeedMult : 5.f;
                    currentSpeedMult = fclamp(currentSpeedMult + steps, kMinSpeedMult, kMaxSpeedMult);
                    looper.SetRate(looper.IsDualMode() ? currentLooper : StereoLooper::BOTH, currentSpeedMult);
                    break;
                }
                case Page::START:
                {
                    int32_t currentLoopStart{looper.GetLoopStart(currentLooper)};
                    int samples{e.asEncoderTurned.increments};
                    currentLoopStart += samples * std::floor(looper.GetBufferSamples(currentLooper) * 0.05f);
                    looper.SetLoopStart(looper.IsDualMode() ? currentLooper : StereoLooper::BOTH, currentLoopStart);
                    break;
                }
                case Page::LENGTH:
                {
                    // TODO: micro-steps for v/oct.
                    int32_t currentLoopLength{looper.GetLoopLength(currentLooper)};
                    int samples{e.asEncoderTurned.increments};
                    samples *= (currentLoopLength >= kMinSamplesForTone) ? std::floor(currentLoopLength * 0.1f) : kMinLoopLengthSamples;
                    currentLoopLength += samples;
                    looper.SetLoopLength(looper.IsDualMode() ? currentLooper : StereoLooper::BOTH, currentLoopLength);
                    break;
                }
                case Page::MOVEMENT:
                {
                    Movement movement = static_cast<Movement>(fclamp(MovementToMenu(currentLooper) + e.asEncoderTurned.increments, 0, 3));
                    MenuToMovement(movement);
                    break;
                }
                case Page::MODE:
                {
                    bool isDualMode{looper.IsDualMode()};
                    int currentMode{looper.GetMode() + e.asEncoderTurned.increments};
                    looper.SetMode(static_cast<StereoLooper::Mode>(fclamp(currentMode, 0, StereoLooper::Mode::LAST_MODE - 1)));
                    if (isDualMode && !looper.IsDualMode())
                    {
                        currentLooper = 0;
                    }
                    break;
                }
                default:
                    break;
                }
            }
            else if (!buttonPressed && !looper.IsBuffering())
            {
                int currentPage{selectedPage + e.asEncoderTurned.increments};
                selectedPage = static_cast<Page>(fclamp(currentPage, 0, Page::LAST_PAGE - 1));
            }
            break;

        case UiEventQueue::Event::EventType::encoderActivityChanged:
            //looper.SetReading(!static_cast<bool>(e.asEncoderActivityChanged.newActivityType));
            break;

        default:
            break;
        }
    }

    void UpdateControls()
    {
        ProcessControls();

        if (!looper.IsStartingUp())
        {
            // Handle CV1 as trigger input for resetting the read position to
            // the loop start point.
            looper.hasCvRestart = isCv1Connected;
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
        if (hw.encoder.RisingEdge())
        {
            eventQueue.AddButtonPressed(0, 1);
        }

        if (hw.encoder.FallingEdge())
        {
            eventQueue.AddButtonReleased(0);
        }

        if (!looper.IsBuffering())
        {
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