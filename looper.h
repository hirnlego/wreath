#pragma once

#include <stdint.h>
#include "dsp.h"

#define MIN_LENGTH 12 // Minimum loop length in samples

namespace wreath 
{
    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        enum class Mode
        {
            MIMEO,
            MODE2,
            MODE3,
        };

        enum class Direction
        {
            FORWARD,
            BACKWARDS,
            PENDULUM,
            DRUNK,
            RANDOM,
        };

        void Init(float *mem, size_t size);
        void ResetBuffer();
        void ToggleFreeze();
        void ChangeLoopLength(float length);
        void StopBuffering();
        float Process(const float input, const int currentSample);

    private:
        float Read(float pos, bool fade = true);
        void Write(size_t pos, float value);

        float *buffer_;
        size_t initBufferSize_;
        size_t bufferSize_;
        float readPos_;
        size_t writePos_;
        size_t loopStart_;
        size_t loopEnd_;
        size_t loopLength_;
        size_t fadeIndex_;
        bool freeze_;
        float feedback_;
        bool feedbackPickup_;
        float dryWet_;
        float speed_;
        int stage_;
        Mode mode_;
        Direction direction_;
        bool forward_;
    };
}  // namespace wreath