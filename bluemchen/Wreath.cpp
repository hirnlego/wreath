#include "../../kxmx_bluemchen/src/kxmx_bluemchen.h"
#include "../looper.h"
#include <string>

using namespace kxmx;
using namespace wreath;
using namespace daisy;

Bluemchen bluemchen;

#define BUFFER_SECONDS 60 // 60 seconds
#define MAX_PAGES 5
#define MIN_LENGTH 12 // Minimum loop length in samples

constexpr size_t BUFFER_SAMPLES = 48000 * BUFFER_SECONDS; // 60 seconds at 48kHz

float DSY_SDRAM_BSS buffer_l[BUFFER_SAMPLES];
float DSY_SDRAM_BSS buffer_r[BUFFER_SAMPLES];

float Clamp(float x, float upper, float lower)
{
    return std::min(upper, std::max(x, lower));
}

float Crossfade(float a, float b, float fade)
{
    return a + (b - a) * fade;
}

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

const char *pageNames[MAX_PAGES] = {
    "Wreath",
    "Speed",
    "Length",
    "Direction",
    "Mode",
};
int currentPage = 0;
bool pageSelected = false;
enum class MenuClickOp
{
    STOP,
    MENU,
    FREEZE,
    RESET,
};
MenuClickOp clickOp = MenuClickOp::STOP;
bool buttonPressed = false;

const char *directionNames[4] = {
    "Forward",
    "Backwards",
    "Pendulum",
    "Random",
};
const char *modeNames[1] = {
    "Mimeo",
};

void UpdateControls()
{
    bluemchen.ProcessAllControls();

    knob1.Process();
    knob2.Process();

    // Update loopers values.
    loopers[0].dryWet = knob1.Value();
    loopers[1].dryWet = knob1.Value();
    loopers[0].feedback = knob2.Value();
    loopers[1].feedback = knob2.Value();

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

    if (loopers[0].stage == -1) {
        str = "Wait...";
        bluemchen.display.SetCursor(0, 24);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    } else if (loopers[0].stage == 0) {
        // Buffering...
        str = "Enc stops";
        bluemchen.display.SetCursor(0, 8);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
        str = "buffering";
        bluemchen.display.SetCursor(0, 16);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
        // Write seconds buffered.
        float seconds = loopers[0].bufferSize / 48000.f;
        float frac = seconds - (int)seconds;
        float inte = seconds - frac;
        str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10)) + " / " + std::to_string(BUFFER_SECONDS);
        bluemchen.display.SetCursor(0, 24);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    } else {
        step = width / (float)loopers[0].bufferSize;

        if (!pageSelected) {
            // Write position in seconds.
            float seconds = loopers[0].readPos / 48000.f;
            float frac = seconds - (int)seconds;
            float inte = seconds - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
            bluemchen.display.SetCursor(0, 16);
            bluemchen.display.WriteString(cstr, Font_6x8, true);
        }
        // Draw the loop bar.
        int start = std::floor(loopers[0].loopStart * step);
        int end = std::floor(loopers[0].loopEnd * step);
        bluemchen.display.DrawRect(0, 27, width, 28, false);
        if (loopers[0].loopStart < loopers[0].loopEnd) {
            bluemchen.display.DrawRect(start, 27, end, 28, true, true);
        } else {
            bluemchen.display.DrawRect(0, 27, end, 28, true, true);
            bluemchen.display.DrawRect(start, 27, width, 28, true, true);
        }
        // Draw the read position.
        int cursor = std::floor(loopers[0].readPos * step);
        bluemchen.display.DrawRect(cursor, 25, cursor, 30, true, true);
    }

    if (pageSelected) {
        if (currentPage == 0) {
            str = "github.com/hirnlego";
            bluemchen.display.SetCursor(0, 8);
            bluemchen.display.WriteString(cstr, Font_6x8, true);

            str = "v1.0";
        } else if (currentPage == 1) {
            // Speed.
            int x = std::floor(loopers[0].speed * (width / 2.f));
            if (looper::Mode::MIMEO != loopers[0].mode) {
                x = std::floor(width / 2.f + (loopers[0].speed * (width / 4.f)));
            }
            bluemchen.display.DrawRect(0, 11, x, 12, true, true);

            float frac = loopers[0].speed - (int)loopers[0].speed;
            float inte = loopers[0].speed - frac;
            str = "x" + std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 100));
        } else if (currentPage == 2) {
            // Draw the loop length bar.
            int x = std::floor(loopers[0].loopLength * step);
            bluemchen.display.DrawRect(0, 11, x, 12, true, true);
            // Write the loop length in seconds.
            float loopLength = loopers[0].loopLength / 48000.f;
            float frac = loopLength - (int)loopLength;
            float inte = loopLength - frac;
            str = std::to_string(static_cast<int>(inte)) + "." + std::to_string(static_cast<int>(frac * 10));
        } else if (currentPage == 3) {
            
        }

        bluemchen.display.SetCursor(0, 16);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }

    if (loopers[0].freeze) {
        str = "*";
        bluemchen.display.SetCursor(width - 6, 0);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }

    bluemchen.display.Update();
}

bool buffering = true;

void UpdateMenu()
{   
    if (!buffering) {
        if (pageSelected) {
            if (currentPage == 1) {
                // Page 1: Speed.
                for (int i = 0; i < 2; i++) {
                    if (looper::Mode::MIMEO == loopers[i].mode) {
                        if (bluemchen.encoder.Increment() > 0) {
                            loopers[i].speed += 0.05f;
                        } else if (bluemchen.encoder.Increment() < 0) {
                            loopers[i].speed -= 0.05f;
                        }
                        loopers[i].speed = Clamp(loopers[i].speed, 2.f, 0.f);
                    } else {
                        loopers[i].speed += bluemchen.encoder.Increment() * 0.05f;    
                        loopers[i].speed = Clamp(loopers[i].speed, 2.f, -2.f);
                    }
                }
            } else if (currentPage == 2) {
                // Page 2: Length.
                for (int i = 0; i < 2; i++) {
                    float loopLength = 0.f;
                    int step = loopers[i].loopLength > 880 ? std::floor(loopers[i].loopLength * 0.1) : 12;
                    if (bluemchen.encoder.Increment() > 0 && loopers[i].loopLength < loopers[i].bufferSize) {
                        loopLength = loopers[i].loopLength + step;
                        if (loopLength > loopers[i].bufferSize) {
                            loopLength = loopers[i].bufferSize;
                        }
                    } else if (bluemchen.encoder.Increment() < 0 && loopers[i].loopLength > MIN_LENGTH) {
                        loopLength = loopers[i].loopLength - step;
                        if (loopLength < MIN_LENGTH) {
                            loopLength = MIN_LENGTH;
                        }
                    }
                    if (loopLength > 0.f) {
                        loopers[i].ChangeLoopLength(loopLength);
                    }
                }
            }    
        } else if (!buttonPressed) {
            currentPage += bluemchen.encoder.Increment();
            if (currentPage < 0) {
                currentPage = 0;
            } else if (currentPage > MAX_PAGES - 1) {
                currentPage = MAX_PAGES - 1;
            }
        }

        if (bluemchen.encoder.TimeHeldMs() >= 500.f) {
            clickOp = MenuClickOp::FREEZE;
        }
        if (bluemchen.encoder.TimeHeldMs() >= 2000.f) {
            clickOp = MenuClickOp::RESET;
        }
    }
        
    if (bluemchen.encoder.RisingEdge()) {
        buttonPressed = true;
    }
    if (bluemchen.encoder.FallingEdge()) {
        if (clickOp == MenuClickOp::STOP) {
            // Stop buffering.
            loopers[0].StopBuffering();
            loopers[1].StopBuffering();
            buffering = false;    
            clickOp = MenuClickOp::MENU;
        } else if (clickOp == MenuClickOp::FREEZE) {
            // Toggle freeze.
            loopers[0].ToggleFreeze();
            loopers[1].ToggleFreeze();
            clickOp = MenuClickOp::MENU;
        } else if (clickOp == MenuClickOp::RESET) {
            // ResetBuffer buffers.
            loopers[0].ResetBuffer();
            loopers[1].ResetBuffer();
            buffering = true;   
            clickOp = MenuClickOp::STOP;
        } else {
            pageSelected = !pageSelected;
        }

        buttonPressed = false;
    }
}

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    for (size_t i = 0; i < size; i++)
    {
        float dry_l = IN_L[i];
        float dry_r = IN_R[i];

        float wet_l = loopers[0].Process(dry_l, i);
        float wet_r = loopers[1].Process(dry_r, i);

        OUT_L[i] = dry_l * (1 - loopers[0].dryWet) + wet_l * loopers[0].dryWet;
        OUT_R[i] = dry_r * (1 - loopers[1].dryWet) + wet_r * loopers[1].dryWet;
    }
}

int main(void)
{
    bluemchen.Init();
    bluemchen.StartAdc();

    knob1.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    knob1_dac.Init(bluemchen.controls[bluemchen.CTRL_1], 0.0f, 1.0f, Parameter::LINEAR);
    knob2_dac.Init(bluemchen.controls[bluemchen.CTRL_2], 0.0f, 1.0f, Parameter::LINEAR);

    cv1.Init(bluemchen.controls[bluemchen.CTRL_3], 0.0f, 1.0f, Parameter::LINEAR);
    cv2.Init(bluemchen.controls[bluemchen.CTRL_4], 0.0f, 1.0f, Parameter::LINEAR);

    loopers[0].Init(buffer_l, BUFFER_SAMPLES);
    loopers[1].Init(buffer_r, BUFFER_SAMPLES);

    bluemchen.StartAudio(AudioCallback);

    while (1) {
        UpdateControls();
        UpdateOled();
        UpdateMenu();
    }
}
