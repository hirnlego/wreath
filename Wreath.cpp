#include "daisysp.h"
#include "../kxmx_bluemchen/src/kxmx_bluemchen.h"

using namespace kxmx;
using namespace daisy;
using namespace daisysp;

Bluemchen bluemchen;

#define sBuffSize 5         // 5 seconds
#define kBuffSize 48000 * 5 // 5 seconds at 48kHz
#define MAX_PAGES 5
#define MIN_LENGTH 12 // Minimum loop length in samples

float DSY_SDRAM_BSS buffer_l[kBuffSize];
float DSY_SDRAM_BSS buffer_r[kBuffSize];

float Clamp(float x, float upper, float lower)
{
    return std::min(upper, std::max(x, lower));
}

float Crossfade(float a, float b, float fade)
{
    return a + (b - a) * fade;
}

struct looper
{
    enum class Mode
    {
        MIMEO,
        MODE2,
        MODE3,
    };
    Mode mode;

    enum class Direction
    {
        FORWARD,
        BACKWARDS,
        PENDULUM,
        RANDOM,
    };
    Direction direction;
    bool forward;

    float *buffer;
    size_t bufferSize;

    float readPos;
    size_t writePos;
    size_t loopStart;
    size_t loopEnd;
    size_t length;
    
    size_t fadeIndex;
    
    bool freeze;
    float feedback;
    bool feedbackPickup;
    float dryWet;

    float speed;

    int stage;

    void ResetBuffer()
    {
        std::fill(&buffer[0], &buffer[bufferSize - 1], 0);
        stage = 0;
        freeze = false;
        feedback = 0;
        feedbackPickup = false;
        readPos = 0;
        writePos = 0;
        loopStart = 0;
        loopEnd = 0;
        speed = 1.f;
    }

    void Init(float *mem, size_t size)
    {
        buffer = mem;
        bufferSize = size;
        ResetBuffer();

        stage = -1;

        mode = Mode::MIMEO;
        direction = Direction::BACKWARDS;
        forward = Direction::FORWARD == direction;
    }

    void ToggleFreeze()
    {
        freeze = !freeze;
        if (freeze) {
            feedbackPickup = false;
            // Frozen.
            if (readPos < loopStart) {
                //readPos = loopStart;
            }
        } else {
            // Not frozen anymore.
            loopStart = 0;
        }
    }

    void ChangeLength(float l)
    {
        length = l;
        loopEnd = length - 1;
    }

    // Reads from a specified point in the delay line using linear interpolation.
    // Also applies a fade in and out to the loop.
    float Read(float pos, bool fade = true)
    {
        float a, b, frac;
        uint32_t i_idx = static_cast<uint32_t>(pos);
        frac = pos - i_idx;
        a = buffer[i_idx];
        b = buffer[(i_idx + (forward ? 1 : -1)) % static_cast<uint32_t>(length)];

        float val = a + (b - a) * frac;

        float samples = length > 1200 ? 1200 : length / 2.f;
        float kWindowFactor = (1.f / samples);
        if (forward) {
            // When going forward we consider read and write position aligned.
            if (pos < samples) {
                // Fade in.
                val = std::sin(HALFPI_F * pos * kWindowFactor) * val;
            } else if (pos >= length - samples) {
                // Fade out.
                val = std::sin(HALFPI_F * (length - pos) * kWindowFactor) * val;
            }
        } else {
            // When going backwards read and write position cross in the middle and at beginning/end.
            float diff = pos - writePos;
            if (diff > 0 && diff < samples) {
                // Fade in.
                val = std::sin(HALFPI_F * diff * kWindowFactor) * val;
            }
        }
        
        
        return val;
    }
    
    void Write(size_t pos, float value)
    {
        buffer[pos] = value;
    }

    float Process(const float input, const int currentSample)
    {
        float readSig = 0.f;

        // Wait a few samples to avoid potential clicking on startup.
        if (-1 == stage) {
            fadeIndex += 1;
            if (fadeIndex > 48000) {
                stage++;
            }
        }

        // Fill up the buffer the first time.
        if (0 == stage) {   
            Write(writePos, input);
            writePos += 1;
            length = writePos;
            
            // Handle end of buffer.
            if (writePos > bufferSize - 1) {
                length = bufferSize;
                loopStart = 0;
                loopEnd = length - 1;
                readPos = forward ? loopStart : loopEnd;
                writePos = 0;
                stage++;
            }   
        } 

        if (stage == 1) {
            readSig = Read(readPos);
            
            if (Mode::MIMEO == mode) {
                if (freeze) {
                    // When frozen, the feedback knob sets the starting point. No writing is done.
                    size_t start = std::floor(feedback * bufferSize);
                    // Pick up where the loop start point is.
                    if (std::abs((int)start - (int)loopStart) < MIN_LENGTH && !feedbackPickup) {
                        feedbackPickup = true;
                    }
                    if (feedbackPickup) {
                        loopStart = start;
                    }
                    if (loopStart + length > bufferSize) {
                        loopEnd = loopStart + length - bufferSize;    
                    } else {
                        loopEnd = loopStart + length - 1;
                    }
                } else {
                    // Clamp to prevent feedback going out of control.
                    Write(writePos, Clamp(input + (readSig * feedback), 1, -1));
                }

                // Always write forward at original speed.
                writePos += 1;
                if (writePos > loopEnd) {
                    writePos = loopStart;
                }
            } else {
                float readSig2 = Read(writePos);
                // In this mode there always is writing, but when frozen writes the looped signal. 
                float writeSig = freeze ? readSig : input + (readSig2 * feedback);
                // Clamp to prevent feedback going out of control.
                Write(writePos, Clamp(writeSig, 1, -1));
                // Always write forward at single speed.
                writePos += 1;
                if (writePos > bufferSize - 1) {
                    writePos = 0;
                }
            }

            // Move the reading position.
            readPos = forward ? readPos + speed : readPos - speed;

            // Handle normal loop boundaries.
            if (loopEnd > loopStart) {
                // Forward direction.
                if (forward && readPos > loopEnd) {
                    readPos = loopStart;
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction) {
                        readPos = loopEnd;
                        forward = !forward;
                    }
                } 
                // Backwards direction.
                else if (!forward && readPos < loopStart) {
                    readPos = loopEnd;
                    // Invert direction when in pendulum.
                    if (Direction::PENDULUM == direction) {
                        readPos = loopStart;
                        forward = !forward;
                    }
                }

            } 
            // Handle inverted loop boundaries (end point comes before start point).
            else {
                if (forward) {
                    if (readPos > bufferSize) {
                        // Wrap-around.
                        readPos = 0;
                    } else if (readPos > loopEnd && readPos < loopStart) {
                        readPos = loopStart;
                        // Invert direction when in pendulum.
                        if (Direction::PENDULUM == direction) {
                            readPos = loopEnd;
                            forward = !forward;
                        }
                    } 
                } else {
                    if (readPos < 0) {
                        // Wrap-around.
                        readPos = bufferSize - 1;
                    } else if (readPos > loopEnd && readPos < loopStart) {
                        readPos = loopEnd;
                        // Invert direction when in pendulum.
                        if (Direction::PENDULUM == direction) {
                            readPos = loopStart;
                            forward = !forward;
                        }
                    } 
                }
            }
        }

        return readSig;
    }
};

// Dry/wet
Parameter knob1;
Parameter knob1_dac;
Parameter cv1; // Dry/wet

// Feedback
Parameter knob2;
Parameter knob2_dac;

// Clock
Parameter cv2;

looper loopers[2];

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
    NORMAL,
    FREEZE,
    ResetBuffer,
};
MenuClickOp clickOp = MenuClickOp::NORMAL;
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
    bluemchen.display.WriteString(cstr, Font_6x8, true);

    float step = width / (float)loopers[0].bufferSize;

    size_t pos = 0 == loopers[0].stage ? loopers[0].writePos : loopers[0].readPos;
    int cursor = std::floor(pos * step);
    bluemchen.display.DrawRect(cursor, 10, cursor, 14, true, true);

    if (currentPage == 0) {
        int cursor = std::floor(loopers[0].writePos * step);
        bluemchen.display.DrawRect(cursor, 26, cursor, 30, true, true);
    }

    if (pageSelected) {
        if (currentPage == 1) {
            int x = std::floor(loopers[0].speed * (width / 2.f));
            if (looper::Mode::MIMEO != loopers[0].mode) {
                x = std::floor(width / 2.f + (loopers[0].speed * (width / 4.f)));
            }
            bluemchen.display.DrawRect(0, 18, x, 22, true, true);
        } else if (currentPage == 2) {
            str = std::to_string(loopers[0].length);
            bluemchen.display.SetCursor(0, 24);
            bluemchen.display.WriteString(cstr, Font_6x8, true);
            int start = std::floor(loopers[0].loopStart * step);
            int end = std::floor(loopers[0].loopEnd * step);
            if (loopers[0].loopStart < loopers[0].loopEnd) {
                bluemchen.display.DrawRect(start, 18, end, 22, true, true);
            } else {
                bluemchen.display.DrawRect(0, 18, end, 22, true, true);
                bluemchen.display.DrawRect(start, 18, width, 22, true, true);
            }
        } else if (currentPage == 3) {
            
        }
    }

    if (loopers[0].freeze) {
        str = "*";
        bluemchen.display.SetCursor(width - 6, 0);
        bluemchen.display.WriteString(cstr, Font_6x8, true);
    }

    bluemchen.display.Update();
}

void UpdateMenu()
{    
    if (pageSelected) {
        if (currentPage == 1) {
            // Page 1: Speed.
            for (int i = 0; i < 2; i++) {
                if (looper::Mode::MIMEO == loopers[i].mode) {
                    if (bluemchen.encoder.Increment() > 0) {
                        loopers[i].speed += 0.1f;
                    } else if (bluemchen.encoder.Increment() < 0) {
                        loopers[i].speed -= 0.1f;
                    }
                    loopers[i].speed = Clamp(loopers[i].speed, 2.f, 0.f);
                } else {
                    loopers[i].speed += bluemchen.encoder.Increment() * 0.1f;    
                    loopers[i].speed = Clamp(loopers[i].speed, 2.f, -2.f);
                }
            }
        } else if (currentPage == 2) {
            // Page 2: Length.
            for (int i = 0; i < 2; i++) {
                float length = 0.f;
                int step = loopers[i].length > 9600 ? 9600 : (loopers[i].length > 480 ? 480 : 12);
                if (bluemchen.encoder.Increment() > 0 && loopers[i].length < loopers[i].bufferSize) {
                    length = loopers[i].length + step;
                    if (length > loopers[i].bufferSize) {
                        length = loopers[i].bufferSize;
                    }
                } else if (bluemchen.encoder.Increment() < 0 && loopers[i].length > MIN_LENGTH) {
                    length = loopers[i].length - step;
                    if (length < MIN_LENGTH) {
                        length = MIN_LENGTH;
                    }
                }
                if (length > 0.f) {
                    loopers[i].ChangeLength(length);
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
        clickOp = MenuClickOp::ResetBuffer;
    }

    if (bluemchen.encoder.RisingEdge()) {
        buttonPressed = true;
    }
    if (bluemchen.encoder.FallingEdge()) {
        if (clickOp == MenuClickOp::FREEZE) {
            // Toggle freeze.
            loopers[0].ToggleFreeze();
            loopers[1].ToggleFreeze();
        } else if (clickOp == MenuClickOp::ResetBuffer) {
            // ResetBuffer buffers.
            loopers[0].ResetBuffer();
            loopers[1].ResetBuffer();
        } else {
            pageSelected = !pageSelected;
        }

        clickOp = MenuClickOp::NORMAL;
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

    loopers[0].Init(buffer_l, kBuffSize);
    loopers[1].Init(buffer_r, kBuffSize);

    bluemchen.StartAudio(AudioCallback);

    while (1) {
        UpdateControls();
        UpdateOled();
        UpdateMenu();
    }
}
