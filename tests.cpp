#include "head.h"
#include "looper.h"
#include <cstdlib>
#include <iostream>
#include <cassert>
#include <cstdint>

using namespace wreath;

const double pi() { return std::atan(1) * 4; }

constexpr int32_t bufferSamples = 48000;

float buffer[48000];
float buffer2[48000];
Looper looper;

bool Compare (float a, float b)
{
    return std::fabs(a - b) <= ( (std::fabs(a) < std::fabs(b) ? std::fabs(b) : std::fabs(a)) * std::numeric_limits<float>::epsilon());
}

float Sine(float f, int32_t t)
{
    return std::sin(2 * pi() * f * t);
}

void Buffer(bool broken)
{
    float f = 1.f / bufferSamples;
    for (size_t i = 0; i < bufferSamples; i++)
    {
        float value = Sine(f, i);
        if (broken && i > bufferSamples / 2.f)
        {
            value = Sine(f, i + 0.25f);
        }
        //std::cout << i << ": " << value << "\n";
        looper.Buffer(value);
    }
    //std::cout << "\n";
    looper.StopBuffering();
}

std::string MapMovement(Movement movement)
{
    switch (movement)
    {
    case Movement::NORMAL:
        return "Normal";
        break;
    case Movement::DRUNK:
        return "Drunk";
        break;
    case Movement::PENDULUM:
        return "Pendulum";
        break;
    default:
        break;
    }
}

std::string MapDirection(Direction direction)
{
    switch (direction)
    {
    case Direction::FORWARD:
        return "Forward";
        break;
    case Direction::BACKWARDS:
        return "Backwards";
        break;
    default:
        break;
    }
}

void TestBoundaries()
{
    Buffer(false);

    struct Scenario
    {
        std::string desc{};
        float loopLength{100.f};
        float loopStart{};
        float index{};
        float rate{1.f};
        Movement movement{NORMAL};
        Direction direction{FORWARD};
        float result{};
        int32_t intResult{};
    };

    static Scenario scenarios[] =
    {
        { "1a - Regular, 1x speed, normal, forward, start", 100, 0, 0, 1.f, NORMAL, FORWARD, 1, 1 },
        { "1b - Regular, 1x speed, normal, forward, end", 100, 0, 99, 1.f, NORMAL, FORWARD, 0, 0 },
        { "1c - Regular, 1x speed, normal, backwards, start", 100, 0, 0, 1.f, NORMAL, BACKWARDS, 99, 99 },
        { "1d - Regular, 1x speed, normal, backwards, end", 100, 0, 99, 1.f, NORMAL, BACKWARDS, 98, 98 },

        { "2a - Regular, 0.5x speed, normal, forward, start", 100, 0, 0, 0.5f, NORMAL, FORWARD, 0.5f, 0 },
        { "2b - Regular, 0.5x speed, normal, forward, end", 100, 0, 99, 0.5f, NORMAL, FORWARD, 99.5f, 99 },
        { "2c - Regular, 0.5x speed, normal, forward, almost end", 100, 0, 99.5f, 0.5f, NORMAL, FORWARD, 0, 0 },
        { "2d - Regular, 0.5x speed, normal, backwards, almost start", 100, 0, 0.5f, 0.5f, NORMAL, BACKWARDS, 0, 0 },
        { "2e - Regular, 0.5x speed, normal, backwards, start", 100, 0, 0, 0.5f, NORMAL, BACKWARDS, 99.5f, 99 },
        { "2f - Regular, 0.5x speed, normal, backwards, end", 100, 0, 99, 0.5f, NORMAL, BACKWARDS, 98.5f, 98 },

        { "3a - Regular, 3.6x speed, normal, forward, start", 100, 0, 0, 3.6f, NORMAL, FORWARD, 3.6f, 3 },
        { "3b - Regular, 3.6x speed, normal, forward, end", 100, 0, 99, 3.6f, NORMAL, FORWARD, 2.6f, 2 },
        { "3c - Regular, 3.6x speed, normal, backwards, start", 100, 0, 0, 3.6f, NORMAL, BACKWARDS, 96.4f, 96 },
        { "3d - Regular, 3.6x speed, normal, backwards, end", 100, 0, 99, 3.6f, NORMAL, BACKWARDS, 95.4f, 95 },

        { "4a - Inverted, 1x speed, normal, forward, end buffer", 10000, 40000, 47999, 1.f, NORMAL, FORWARD, 0, 0 },
        { "4b - Inverted, 1x speed, normal, forward, end loop", 10000, 40000, 1999, 1.f, NORMAL, FORWARD, 40000, 40000 },
        { "4c - Inverted, 1x speed, normal, backwards, start buffer", 10000, 40000, 0, 1.f, NORMAL, BACKWARDS, 47999, 47999 },
        { "4d - Inverted, 1x speed, normal, backwards, start loop", 10000, 40000, 40000, 1.f, NORMAL, BACKWARDS, 1999, 1999 },

        { "5a - Inverted, 0.4x speed, normal, forward, end buffer", 10000, 40000, 47999, 0.4f, NORMAL, FORWARD, 47999.4f, 47999 },
        { "5b - Inverted, 0.4x speed, normal, forward, end loop", 10000, 40000, 1999, 0.4f, NORMAL, FORWARD, 1999.4f, 1999 },
        { "5c - Inverted, 0.4x speed, normal, backwards, start buffer", 10000, 40000, 0, 0.4f, NORMAL, BACKWARDS, 47999.6f, 47999 },
        { "5d - Inverted, 0.4x speed, normal, backwards, start loop", 10000, 40000, 40000, 0.4f, NORMAL, BACKWARDS, 1999.6f, 1999 },

        { "6a - Inverted, 3.6x speed, normal, forward, end buffer", 10000, 40000, 47999, 3.6f, NORMAL, FORWARD, 2.6f, 2 },
        { "6b - Inverted, 3.6x speed, normal, forward, end loop", 10000, 40000, 1999, 3.6f, NORMAL, FORWARD, 40002.6f, 40002 },
        { "6c - Inverted, 3.6x speed, normal, backwards, start buffer", 10000, 40000, 0, 3.6f, NORMAL, BACKWARDS, 47996.4f, 47996 },
        { "6d - Inverted, 3.6x speed, normal, backwards, start loop", 10000, 40000, 40000, 3.6f, NORMAL, BACKWARDS, 1996.4f, 1996 },

        { "7a - Regular, 49.7x speed, normal, forward, start", 100, 0, 0, 49.7f, NORMAL, FORWARD, 49.7f, 49 },
        { "7b - Regular, 49.7x speed, normal, forward, almost end", 100, 0.f, 99.4f, 49.7f, NORMAL, FORWARD, 49.1f, 49 },
        { "7c - Regular, 49.7x speed, normal, backwards, end", 100, 0, 99, 49.7f, NORMAL, BACKWARDS, 49.3, 49 },
        { "7d - Regular, 49.7x speed, normal, backwards, almost start", 100, 0, 11.2f, 49.7f, NORMAL, BACKWARDS, 61.5f, 61 },

        { "8a - Regular, 1x speed, pendulum, forward, start", 100, 0, 0, 1, PENDULUM, BACKWARDS, 1, 1 },
        { "8b - Regular, 1x speed, pendulum, forward, end", 100, 0, 99, 1, PENDULUM, FORWARD, 98, 98 },
    };

    std::cout << "\n";

    for (Scenario scenario : scenarios)
    {
        looper.Reset();
        looper.SetTriggerMode(Looper::TriggerMode::LOOP);
        looper.Start(true);
        std::cout << "Scenario " << scenario.desc << "\n";
        looper.SetLoopStart(scenario.loopStart);
        std::cout << "Loop start: " << scenario.loopStart << "\n";
        looper.SetLoopLength(scenario.loopLength);
        std::cout << "Loop length: " << scenario.loopLength << "\n";
        std::cout << "Loop end: " << looper.GetLoopEnd() << "\n";
        looper.SetReadRate(scenario.rate);
        std::cout << "Rate: " << scenario.rate << "\n";
        looper.SetMovement(scenario.movement);
        std::cout << "Movement: " << MapMovement(scenario.movement) << "\n";
        looper.SetDirection(scenario.direction);
        std::cout << "Direction: " << MapDirection(scenario.direction) << "\n";
        float index = scenario.index;
        looper.SetReadPos(index);
        std::cout << "Current index: " << index << "\n";
        std::cout << "Next index (before fix): " << index + (scenario.rate * scenario.direction) << "\n";
        looper.UpdateReadPos();
        index = looper.GetReadPos();
        std::cout << "Next index (float): " << index << " (expected " << scenario.result << ")\n";
        int32_t intIndex = static_cast<int32_t>(std::floor(index));
        std::cout << "Next index (int): " << intIndex << " (expected " << scenario.intResult << ")\n";
        std::cout << "\n";
        //assert(Compare(index, scenario.result));
        assert(Compare(intIndex, scenario.intResult));
    }
}

void TestRead()
{
    Buffer(false);

    float index = bufferSamples - 1;

    looper.SetReadPos(index);
    float value = looper.Read(0);
    std::cout << "Value at " << index << ": " << value << "\n";
}

void TestFade()
{
    Buffer(false);
}

int main()
{
    looper.Init(48000, buffer, buffer2, 48000);

    TestBoundaries();
    //TestRead();
    //TestFade();

    return 0;
}
