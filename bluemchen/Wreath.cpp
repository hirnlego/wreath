#include <string>
#include <string.h>
#include "kxmx_bluemchen.h"
#include "dsp.h"
#include "../looper.h"

using namespace kxmx;
using namespace wreath;
using namespace daisy;
using namespace daisysp;

Bluemchen bluemchen;

#define MAX_PAGES 5

constexpr size_t kSampleRate{48000};
constexpr int kBufferSeconds{120}; // 2 minutes max

const size_t kBufferSamples{kSampleRate * kBufferSeconds};

float DSY_SDRAM_BSS buffer_l[kBufferSamples];
float DSY_SDRAM_BSS buffer_r[kBufferSamples];

// Dry/wet
Parameter knob1;
Parameter knob1_dac;
Parameter cv1; // Dry/wet

// Feedback
Parameter knob2;
Parameter knob2_dac;

// Clock
Parameter cv2;

Looper loopers[2];

bool buffering{true};
float dryWet{};
float feedback{};

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

/** The DSY_QSPI_BSS attribute places your array in QSPI memory */

float DSY_QSPI_BSS qspi_buffer[5];

struct CONFIGURATION
{
};

CONFIGURATION curent_config;

// https://forum.electro-smith.com/t/persisting-data-to-from-flash/502/27
void SaveConfig(uint32_t slot)
{
    /*
    size_t size = sizeof(wavform_ram[0]) * WAVE_LENGTH;
    // Grab physical address from pointer
    size_t address = (size_t)qspi_buffer;
    // Erase qspi and then write that wave
    hw.qspi.Erase(address, address + size);
    hw.qspi.Write(address, size, (uint8_t *)wavform_ram);
    */

    uint32_t base = 0x90000000;
    base += slot * 4096; // works only because sizeof(CONFIGURATION) < 4096
    bluemchen.seed.qspi.Erase(base, base + sizeof(CONFIGURATION));
    bluemchen.seed.qspi.Write(base, sizeof(CONFIGURATION), (uint8_t *)&curent_config);
}

void LoadConfig(uint32_t slot)
{
    memcpy(&curent_config, reinterpret_cast<void *>(0x90000000 + (slot * 4096)), sizeof(CONFIGURATION));
}

void UpdateControls()
{
    bluemchen.ProcessAllControls();

    knob1.Process();
    knob2.Process();

    // Update loopers values.
    if (std::abs(dryWet - knob1.Value()) > 0.01f)
    {
        dryWet = knob1.Value();
        loopers[0].SetDryWet(dryWet);
        loopers[1].SetDryWet(dryWet);
    }

    if (std::abs(feedback - knob2.Value()) > 0.01f)
    {
        feedback = knob2.Value();
        loopers[0].SetFeedback(feedback);
        loopers[1].SetFeedback(feedback);
    }

    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::ONE, static_cast<uint16_t>(knob1_dac.Process()));
    bluemchen.seed.dac.WriteValue(daisy::DacHandle::Channel::TWO, static_cast<uint16_t>(knob2_dac.Process()));

    cv1.Process();
    cv2.Process();
}

void UpdateOled()
{
    int width = bluemchen.display.Width();

    bluemchen.display.Fill(false);

    std::string str = pageNames[currentPage];
    char *cstr = &str[0];
    bluemchen.display.SetCursor(0, 0);
    bluemchen.display.WriteString(cstr, Font_6x8, !pageSelected);

    float step = width;

    if (loopers[0].IsStartingUp())
    {
        str = "Wait...";
        bluemchen.display.SetCursor(0, 24);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }
    else if (loopers[0].IsBuffering())
    {
        // Buffering...
        str = "Enc stops";
        bluemchen.display.SetCursor(0, 8);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
        str = "buffering";
        bluemchen.display.SetCursor(0, 16);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
        // Write seconds buffered.
        float seconds = loopers[0].GetBufferSeconds();
        float frac = seconds - (int)seconds;
        float inte = seconds - frac;
        str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + " / " + std::to_string(kBufferSeconds);
        bluemchen.display.SetCursor(0, 24);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }
    else
    {
        step = width / (float)loopers[0].GetBufferSamples();

        if (!pageSelected)
        {
            // Write position in seconds.
            float seconds = loopers[0].GetPositionSeconds();
            float frac = seconds - (int)seconds;
            float inte = seconds - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
            bluemchen.display.SetCursor(0, 16);
            bluemchen.display.WriteString(cstr, Font_6x8, true);
        }
        // Draw the loop bar.
        int start = std::floor(loopers[0].GetLoopStart() * step);
        int end = std::floor(loopers[0].GetLoopEnd() * step);
        if (loopers[0].GetLoopStart() < loopers[0].GetLoopEnd())
        {
            bluemchen.display.DrawRect(start, 27, end, 28, true, true);
        }
        else
        {
            bluemchen.display.DrawRect(0, 27, end, 28, true, true);
            bluemchen.display.DrawRect(start, 27, width, 28, true, true);
        }
        // Draw the read position.
        int cursor = std::floor(loopers[0].GetPosition() * step);
        bluemchen.display.DrawRect(cursor, 25, cursor, 30, true, true);
    }

    if (pageSelected)
    {
        if (currentPage == 0)
        {
            str = "github.com/hirnlego";
            bluemchen.display.SetCursor(0, 8);
            bluemchen.display.WriteString(cstr, Font_6x8, true);

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
            bluemchen.display.DrawRect(0, 11, x, 12, true, true);

            float frac = loopers[0].GetSpeed() - (int)loopers[0].GetSpeed();
            float inte = loopers[0].GetSpeed() - frac;
            str = "x" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100));
        }
        else if (currentPage == 2)
        {
            // Page 2: Length.
            // Draw the loop length bar.
            int x = std::floor(loopers[0].GetLoopLength() * step);
            bluemchen.display.DrawRect(0, 11, x, 12, true, true);
            // Write the loop length in seconds.
            float loopLength = loopers[0].GetLoopLength() / (float)kSampleRate;
            float frac = loopLength - (int)loopLength;
            float inte = loopLength - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
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

        bluemchen.display.SetCursor(0, 16);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }

    if (loopers[0].IsFrozen())
    {
        str = "*";
        bluemchen.display.SetCursor(width - 6, 0);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }

    bluemchen.display.Update();
}

void UpdateMenu()
{
    if (!buffering)
    {
        if (pageSelected)
        {
            if (currentPage == 1)
            {
                // Page 1: Speed.
                for (int i = 0; i < 2; i++)
                {
                    if (loopers[0].IsMimeoMode())
                    {
                        if (bluemchen.encoder.Increment() > 0)
                        {
                            loopers[i].SetSpeed(loopers[i].GetSpeed() + 0.05f);
                        }
                        else if (bluemchen.encoder.Increment() < 0)
                        {
                            loopers[i].SetSpeed(loopers[i].GetSpeed() - 0.05f);
                        }
                    }
                }
            }
            else if (currentPage == 2)
            {
                // Page 2: Length.
                int step{};
                for (int i = 0; i < 2; i++)
                {
                    // TODO: micro-steps for v/oct. Also, step should always be integer!
                    step = loopers[i].GetLoopLength() > 880 ? std::floor(loopers[i].GetLoopLength() * 0.1) : 12;
                    if (bluemchen.encoder.Increment() > 0)
                    {
                        loopers[i].IncrementLoopLength(step);
                    }
                    else if (bluemchen.encoder.Increment() < 0)
                    {
                        loopers[i].DecrementLoopLength(step);
                    }
                }
            }
            else if (currentPage == 3)
            {
                // Page 3: Movement.
                for (int i = 0; i < 2; i++)
                {
                    int currentMovement{loopers[i].GetMovement()};
                    currentMovement += bluemchen.encoder.Increment();
                    loopers[i].SetMovement(static_cast<Looper::Movement>(fclamp(currentMovement, 0, Looper::Movement::LAST_MOVEMENT - 1)));
                }
            }
            else if (currentPage == 4)
            {
                // Page 4: Mode.
                for (int i = 0; i < 2; i++)
                {
                    int currentMode{loopers[i].GetMode()};
                    currentMode += bluemchen.encoder.Increment();
                    loopers[i].SetMode(static_cast<Looper::Mode>(fclamp(currentMode, 0, Looper::Mode::LAST_MODE - 1)));
                }
            }
        }
        else if (!buttonPressed)
        {
            currentPage += bluemchen.encoder.Increment();
            if (currentPage < 0)
            {
                currentPage = 0;
            }
            else if (currentPage > MAX_PAGES - 1)
            {
                currentPage = MAX_PAGES - 1;
            }
        }

        if (bluemchen.encoder.TimeHeldMs() >= 500.f)
        {
            clickOp = MenuClickOp::FREEZE;
        }
        if (bluemchen.encoder.TimeHeldMs() >= 2000.f)
        {
            clickOp = MenuClickOp::RESET;
        }
    }

    if (bluemchen.encoder.RisingEdge())
    {
        buttonPressed = true;
    }
    if (bluemchen.encoder.FallingEdge())
    {
        if (clickOp == MenuClickOp::STOP)
        {
            // Stop buffering.
            loopers[0].StopBuffering();
            loopers[1].StopBuffering();
            buffering = false;
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
            buffering = true;
            clickOp = MenuClickOp::STOP;
        }
        else
        {
            pageSelected = !pageSelected;
        }

        buttonPressed = false;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float dry_l{IN_L[i]};
        float dry_r{IN_R[i]};

        float wet_l{loopers[0].Process(dry_l, i)};
        float wet_r{loopers[1].Process(dry_r, i)};

        OUT_L[i] = dry_l * (1 - dryWet) + wet_l * dryWet;
        OUT_R[i] = dry_r * (1 - dryWet) + wet_r * dryWet;
    }
}

int main(void)
{
    bluemchen.Init();
    bluemchen.SetAudioSampleRate(static_cast<daisy::SaiHandle::Config::SampleRate>(kSampleRate));
    bluemchen.StartAdc();

    knob1.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    knob1_dac.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2_dac.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    cv1.Init(bluemchen.controls[bluemchen.CTRL_3], 0.0f, 1.0f, Parameter::LINEAR);
    cv2.Init(bluemchen.controls[bluemchen.CTRL_4], 0.0f, 1.0f, Parameter::LINEAR);

    loopers[0].Init(kSampleRate, buffer_l, kBufferSeconds);
    loopers[1].Init(kSampleRate, buffer_r, kBufferSeconds);

    bluemchen.StartAudio(AudioCallback);

    while (1)
    {
        UpdateControls();
        UpdateOled();
        UpdateMenu();
    }
}
