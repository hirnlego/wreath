#include "head.h"
#include <cstdlib>
#include <iostream>
#include <cassert>
#include <stdint.h>

using namespace wreath;

int main()
{
    float buffer[48000];
    Head head{Type::READ};
    head.Init(buffer, 48000);
    head.InitBuffer(48000);

    struct Scenario
    {
        int id{};
        std::string desc{};
        int32_t loopLength{100};
        int32_t loopStart{};
        float index{};
        float rate{1.f};
        Movement movement{NORMAL};
        Direction direction{FORWARD};
        float result{};
        int32_t intResult{};
    };

    Scenario scenarios[] =
    {
        { 1, "Regular, 1x speed, normal, forward, start", 100, 0, 0, 1.f, NORMAL, FORWARD, 1, 1 },
        { 2, "Regular, 1x speed, normal, forward, end", 100, 0, 99, 1.f, NORMAL, FORWARD, 0, 0 },
        { 3, "Regular, 1x speed, normal, backwards, start", 100, 0, 0, 1.f, NORMAL, BACKWARDS, 99, 99 },
        { 4, "Regular, 1x speed, normal, backwards, end", 100, 0, 99, 1.f, NORMAL, BACKWARDS, 98, 98 },

        { 5, "Regular, 0.5x speed, normal, forward, start", 100, 0, 0, 0.5f, NORMAL, FORWARD, 0.5f, 1 },
        { 6, "Regular, 0.5x speed, normal, forward, end", 100, 0, 99, 0.5f, NORMAL, FORWARD, 99.5f, 99 },
        { 7, "Regular, 0.5x speed, normal, backwards, start", 100, 0, 0, 0.5f, NORMAL, BACKWARDS, 99.5f, 99 },
        { 8, "Regular, 0.5x speed, normal, backwards, end", 100, 0, 99, 0.5f, NORMAL, BACKWARDS, 98.5f, 98 },

        { 9, "Inverted, 1x speed, normal, forward, end buffer", 10000, 40000, 47999, 1.f, NORMAL, FORWARD, 0, 0 },
        { 10, "Inverted, 1x speed, normal, forward, end loop", 10000, 40000, 1999, 1.f, NORMAL, FORWARD, 40000, 40000 },
    };

    std::cout << "\n";

    for (Scenario scenario : scenarios)
    {
        std::cout << "Scenario " << scenario.id << ": " << scenario.desc << "\n";
        head.SetLoopLength(scenario.loopLength);
        std::cout << "Loop length: " << scenario.loopLength << "\n";
        head.SetLoopStart(scenario.loopStart);
        std::cout << "Loop start: " << scenario.loopStart << "\n";
        std::cout << "Loop end: " << head.GetLoopEnd() << "\n";
        head.SetRate(scenario.rate);
        std::cout << "Rate: " << scenario.rate << "\n";
        head.SetMovement(scenario.movement);
        std::cout << "Movement: " << scenario.movement << "\n";
        head.SetDirection(scenario.direction);
        std::cout << "Direction: " << scenario.direction << "\n";
        float index = scenario.index;
        head.SetIndex(index);
        std::cout << "Current index: " << index << "\n";
        index = head.UpdatePosition();
        std::cout << "Next index (float): " << index << " (expected " << scenario.result << ")\n";
        int32_t intIndex = head.GetIntPosition();
        std::cout << "Next index (int): " << intIndex << " (expected " << scenario.intResult << ")\n";
        std::cout << "\n";
        assert(index == scenario.result);
        assert(intIndex == scenario.intResult);
    }


    return 0;
}
