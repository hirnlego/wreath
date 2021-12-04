#pragma once

#include <string>
#include "hw.h"

namespace wreath
{
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
        "Mode 3",
    };

    void UpdateOled()
    {
        int width = hw.display.Width();

        hw.display.Fill(false);

        std::string str = pageNames[currentPage];
        char *cstr = &str[0];
        hw.display.SetCursor(0, 0);
        hw.display.WriteString(cstr, Font_6x8, !pageSelected);

        float step = width;

        if (loopers[0].IsStartingUp())
        {
            str = "Wait...";
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else if (loopers[0].IsBuffering())
        {
            // Buffering...
            str = "Enc stops";
            hw.display.SetCursor(0, 8);
            hw.display.WriteString(cstr, Font_6x8, true);
            str = "buffering";
            hw.display.SetCursor(0, 16);
            hw.display.WriteString(cstr, Font_6x8, true);
            // Write seconds buffered.
            float seconds = loopers[0].GetBufferSeconds();
            float frac = seconds - (int)seconds;
            float inte = seconds - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + "/" + std::to_string(kBufferSeconds) + "s";
            hw.display.SetCursor(0, 24);
            hw.display.WriteString(cstr, Font_6x8, true);
        }
        else
        {
            step = width / (float)loopers[0].GetBufferSamples();

            if (!pageSelected)
            {
                float loopLength = loopers[0].GetLoopLengthSeconds();
                float position = loopers[0].GetPositionSeconds();

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

                hw.display.SetCursor(0, 16);
                hw.display.WriteString(cstr, Font_6x8, true);
            }
            // Draw the loop bar.
            int start = std::floor(loopers[0].GetLoopStart() * step);
            int end = std::floor(loopers[0].GetLoopEnd() * step);
            if (loopers[0].GetLoopStart() > loopers[0].GetLoopEnd())
            {
                // Normal loop (start position before end position).
                hw.display.DrawRect(start, 27, end, 28, true, true);
            }
            else
            {
                // Inverse loop (end position before start position).
                hw.display.DrawRect(0, 27, end, 28, true, true);
                hw.display.DrawRect(start, 27, width, 28, true, true);
            }
            // Draw the start position depending on the looper movement.
            int cursor{};
            if (Looper::Movement::BACKWARDS != loopers[0].GetMovement())
            {
                cursor = start;
                hw.display.DrawRect(cursor, 25, cursor, 27, true, true);
            }
            // Draw the end position depending on the looper movement.
            if (Looper::Movement::BACKWARDS == loopers[0].GetMovement() || Looper::Movement::PENDULUM == loopers[0].GetMovement())
            {
                cursor = end;
                hw.display.DrawRect(cursor, 25, cursor, 27, true, true);
            }
            // Draw the read position.
            cursor = std::floor(loopers[0].GetPosition() * step);
            hw.display.DrawRect(cursor, 28, cursor, 30, true, true);
        }

        if (pageSelected)
        {
            if (currentPage == 0)
            {
                str = "github.com/hirnlego";
                hw.display.SetCursor(0, 8);
                hw.display.WriteString(cstr, Font_6x8, true);

                str = "v1.0";
            }
            else if (currentPage == 1)
            {
                // Page 1: Speed.
                int x = std::floor(loopers[0].GetSpeed() * (width / 2.f));
                if (!loopers[0].IsMimeoMode())
                {
                    x = std::floor(width / 2.f + (loopers[0].GetSpeed() * (width / 4.f)));
                }
                hw.display.DrawRect(0, 11, x, 12, true, true);

                float frac = loopers[0].GetSpeed() - (int)loopers[0].GetSpeed();
                float inte = loopers[0].GetSpeed() - frac;
                str = "x" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100));
            }
            else if (currentPage == 2)
            {
                // Page 2: Length.
                // Draw the loop length bar.
                int x = std::floor(loopers[0].GetLoopLength() * step);
                hw.display.DrawRect(0, 11, x, 12, true, true);
                // Write the loop length in seconds.
                float loopLength = loopers[0].GetLoopLengthSeconds();
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
                str = movementNames[static_cast<int>(loopers[0].GetMovement())];
            }
            else if (currentPage == 4)
            {
                // Page 4: Mode.
                str = modeNames[static_cast<int>(loopers[0].GetMode())];
            }

            hw.display.SetCursor(0, 16);
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
                    for (int i = 0; i < 2; i++)
                    {
                        if (loopers[i].IsMimeoMode())
                        {
                            if (hw.encoder.Increment() > 0)
                            {
                                loopers[i].SetSpeed(loopers[i].GetSpeed() + 0.05f);
                            }
                            else if (hw.encoder.Increment() < 0)
                            {
                                loopers[i].SetSpeed(loopers[i].GetSpeed() - 0.05f);
                            }
                        }
                    }
                }
                else if (currentPage == 2)
                {
                    // Page 2: Length.
                    int samples{};
                    for (int i = 0; i < 2; i++)
                    {
                        // TODO: micro-steps for v/oct.
                        samples = (loopers[i].GetLoopLength() > 480) ? std::floor(loopers[i].GetLoopLength() * 0.1) : kMinSamples;
                        if (hw.encoder.Increment() > 0)
                        {
                            loopers[i].IncrementLoopLength(samples);
                        }
                        else if (hw.encoder.Increment() < 0)
                        {
                            loopers[i].DecrementLoopLength(samples);
                        }
                    }
                }
                else if (currentPage == 3)
                {
                    // Page 3: Movement.
                    for (int i = 0; i < 2; i++)
                    {
                        int currentMovement{loopers[i].GetMovement()};
                        currentMovement += hw.encoder.Increment();
                        loopers[i].SetMovement(static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                    }
                }
                else if (currentPage == 4)
                {
                    // Page 4: Mode.
                    for (int i = 0; i < 2; i++)
                    {
                        int currentMode{loopers[i].GetMode()};
                        currentMode += hw.encoder.Increment();
                        loopers[i].SetMode(static_cast<Looper::Mode>(fclamp(currentMode, 0, Looper::Mode::LAST_MODE - 1)));
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
                else if (currentPage > MAX_PAGES - 1)
                {
                    currentPage = MAX_PAGES - 1;
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
                loopers[0].StopBuffering();
                loopers[1].StopBuffering();
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
                clickOp = MenuClickOp::STOP;
            }
            else
            {
                pageSelected = !pageSelected;
            }

            buttonPressed = false;
        }
    }
}