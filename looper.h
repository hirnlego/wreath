#pragma once

#include <stdint.h>

#define MIN_LENGTH 12 // Minimum loop length in samples
#define MIN_SPEED -2.f // Minimum speed
#define MAX_SPEED 2.f // Maximum speed

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

        /**
         * @brief Initializes the looper.
         * 
         * @param mem 
         * @param size 
         */
        void Init(float *mem, size_t size);
        /**
         * @brief Resets the buffer.
         */
        void ResetBuffer();
        /**
         * @brief Stops initial buffering and create the working buffer.
         * 
         */
        void StopBuffering();
        /**
         * @brief Processes a sample.
         * 
         * @param input 
         * @param currentSample 
         * @return float 
         */
        float Process(const float input, const int currentSample);

        void SetSpeed(float speed);
        void ToggleFreeze();

        inline size_t GetBufferSamples() { return bufferSamples_; }
        inline size_t GetLoopStart() { return loopStart_; }
        inline size_t GetLoopEnd() { return loopEnd_; }
        inline size_t GetLoopLength() { return loopLength_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetPosition() { return readPos_; }
        inline float GetSpeed() { return speed_; }
        inline bool IsStartingUp() { return -1 == stage_; }
        inline bool IsBuffering() { return 0 == stage_; }
        inline bool IsFrozen() { return freeze_; }
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void IncrementLoopLength(float step)
        {
            float length = 0.f;
            if (loopLength_ < bufferSamples_)
            {
                length = loopLength_ + step;
                if (length > bufferSamples_)
                {
                    length = bufferSamples_;
                }
            }
            SetLoopLength(length);
        };
        inline void DecrementLoopLength(float step)
        {
            float length = 0.f;
            if (loopLength_ > MIN_LENGTH)
            {
                length = loopLength_ - step;
                if (length < MIN_LENGTH)
                {
                    length = MIN_LENGTH;
                }
            }
            SetLoopLength(length);
        };
        inline void SetLoopLength(float length) 
        {   
            loopLength_ = length;
            loopEnd_ = loopLength_ - 1;
        };

    private:
        float Read(float pos);

        inline void Write(size_t pos, float value) { buffer_[pos] = value; }
        inline void SetReadPos(float pos)
        {
            readPos_ = pos;
            readPosSeconds_ = readPos_ / 48000.f;
        }

        float *buffer_;
        size_t initBufferSamples_;
        size_t bufferSamples_;
        float bufferSeconds_;
        float readPos_;
        float readPosSeconds_;
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