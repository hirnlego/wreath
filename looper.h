#pragma once

#include <stdint.h>

namespace wreath
{
    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        enum class State
        {
            INIT,
            BUFFERING,
            RECORDING,
            FROZEN,
        };

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
         * @param sampleRate 
         * @param mem 
         * @param maxBufferSeconds 
         */
        void Init(size_t sampleRate, float *mem, int maxBufferSeconds);
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
        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void IncrementLoopLength(size_t step)
        {
            size_t length = 0;
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
        inline void DecrementLoopLength(size_t step)
        {
            size_t length = 0;
            if (loopLength_ > 0)
            {
                length = loopLength_ - step;
                if (length < 0)
                {
                    length = 0;
                }
            }
            SetLoopLength(length);
        };
        inline void SetLoopLength(size_t length)
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
            readPosSeconds_ = readPos_ / sampleRate_;
        }

        float *buffer_;
        float bufferSeconds_;
        float readPos_;
        float readPosSeconds_;
        float feedback_;
        float dryWet_;
        float speed_;
        size_t initBufferSamples_;
        size_t bufferSamples_;
        size_t writePos_;
        size_t loopStart_;
        size_t loopEnd_;
        size_t loopLength_;
        size_t fadeIndex_;
        size_t sampleRate_;
        bool feedbackPickup_;
        bool freeze_;
        bool forward_;
        State state_;
        Mode mode_;
        Direction direction_;
    };
} // namespace wreath