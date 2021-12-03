#pragma once

#include <stdint.h>

namespace wreath
{
    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        enum State
        {
            INIT,
            BUFFERING,
            RECORDING,
            FROZEN,
        };

        enum Mode
        {
            MIMEO,
            MODE2,
            MODE3,
            LAST_MODE,
        };

        enum Movement
        {
            FORWARD,
            BACKWARDS,
            PENDULUM,
            DRUNK,
            RANDOM,
            LAST_MOVEMENT,
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
        void IncrementLoopLength(size_t step);
        void DecrementLoopLength(size_t step);
        void SetLoopLength(size_t length);

        inline size_t GetBufferSamples() { return bufferSamples_; }
        inline size_t GetLoopStart() { return loopStart_; }
        inline size_t GetLoopEnd() { return loopEnd_; }
        inline size_t GetLoopLength() { return loopLength_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetPosition() { return readPos_; }
        inline float GetSpeed() { return speed_; }
        inline Movement GetMovement() { return movement_; }
        inline Mode GetMode() { return mode_; }
        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void SetMovement(Movement movement) { 
            movement_ = movement; 
            if (Movement::FORWARD == movement && !forward_) {
                forward_ = true;
            }
            else if (Movement::BACKWARDS == movement && forward_)
            {
                forward_ = false;
            }
        }
        inline void SetMode(Mode mode) { mode_ = mode; }
        inline void Restart() { readPos_ = loopStart_; }

    private:
        float Read(float pos);

        inline void Write(size_t pos, float value) { buffer_[pos] = value; }
        inline void SetReadPos(float pos)
        {
            readPos_ = pos;
            readPosSeconds_ = readPos_ / sampleRate_;
        }

        float *buffer_{};
        float bufferSeconds_{};
        float nextPos_{};
        float readPos_{};
        float readPosSeconds_{};
        float feedback_{};
        float dryWet_{};
        float speed_{};
        size_t initBufferSamples_{};
        size_t bufferSamples_{};
        size_t writePos_{};
        size_t loopStart_{};
        size_t loopEnd_{};
        size_t loopLength_{};
        size_t fadeIndex_{};
        size_t sampleRate_{};
        bool feedbackPickup_{};
        bool freeze_{};
        bool forward_{};
        State state_{};
        Mode mode_{};
        Movement movement_{};
    };
} // namespace wreath