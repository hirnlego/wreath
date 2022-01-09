#include "head.h"
#include <cstdlib>
#include <iostream>
#include <stdint.h>

using namespace wreath;

int main()
{
    int32_t loopStart;
    int32_t loopLength;
    float index;

    float buffer[48000];

    Head head{Type::READ};
    head.Init(buffer, 48000);
    head.InitBuffer(48000);
    //head.SetDirection(BACKWARDS);

    std::cout << "Loop start: ";
    std::cin >> loopStart;
    head.SetLoopStart(loopStart);
    std::cout << "Loop length: ";
    std::cin >> loopLength;
    head.SetLoopLength(loopLength);
    std::cout << "Loop end: " << head.GetLoopEnd() << "\n";

    while (true)
    {
        std::cout << "Current index: ";
        std::cin >> index;
        head.SetIndex(index);
        index = head.UpdatePosition();
        std::cout << "Next index (float): " << index;
        std::cout << "\n" << "Next index (int): " << head.GetIntPosition();
        std::cout << "\n";
    }

    return 0;
}
