#pragma once

#include <stdint.h>
#include "Dynamics/crossfade.h"

namespace wreath
{
    using namespace daisysp;

    constexpr int kMinSamples{48};
    constexpr int kFadeSamples{480}; // Note: 480 samples is 10ms @ 48KHz.

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
            DUAL,
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
        void ResetLoopLength();
        void SetMovement(Movement movement);

        inline size_t GetBufferSamples() { return bufferSamples_; }
        inline size_t GetLoopStart() { return loopStart_; }
        inline size_t GetLoopEnd() { return loopEnd_; }
        inline size_t GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetPosition() { return readPos_; }
        inline float GetNextPosition() { return nextReadPos_; }
        inline float GetSpeed() { return speed_; }
        inline Movement GetMovement() { return movement_; }
        inline Mode GetMode() { return mode_; }
        inline bool IsGoingForward() { return forward_; }
        inline bool IsStartingUp() { return State::INIT == state_; }
        inline bool IsBuffering() { return State::BUFFERING == state_; }
        inline bool IsRecording() { return State::RECORDING == state_; }
        inline bool IsFrozen() { return State::FROZEN == state_; }
        inline bool IsMimeoMode() { return Mode::MIMEO == mode_; }
        inline bool IsMode2Mode() { return Mode::MODE2 == mode_; }
        inline bool IsDualMode() { return Mode::DUAL == mode_; }
        inline void SetDryWet(float dryWet) { dryWet_ = dryWet; }
        inline void SetFeedback(float feedback) { feedback_ = feedback; }
        inline void SetMode(Mode mode) { mode_ = mode; }
        inline void Restart() { mustRestart_ = true; }

    private:
        float Read(float pos);
        size_t GetRandomPosition();
        void SetWritePos(float pos);
        void SetReadPos(float pos);
        void SetReadPosAtStart();
        void SetReadPosAtEnd();
        void HandlePosBoundaries(float &pos, bool isReadPos);

        inline void Write(size_t pos, float value) { buffer_[pos] = value; }
        inline float GetFadeSamples() { return (loopLength_ > kFadeSamples * 2) ? kFadeSamples : loopLength_ / 2.f; }

        float *buffer_{}; // The buffer
        float bufferSeconds_{}; // Written buffer length in seconds
        float readPos_{}; // The read position
        float originalReadPos_{}; // The read position
        float readPosSeconds_{}; // Read position in seconds
        float nextReadPos_{}; // Next read position
        float fadePos_{}; // Fade position
        float loopLengthSeconds_{}; // Length of the loop in seconds
        float feedback_{}; // Feedback amount for sound-on-sound
        float dryWet_{}; // Dry/wet balance
        float speed_{}; // Speed multiplier
        size_t initBufferSamples_{}; // The whole buffer length in samples
        size_t bufferSamples_{}; // The written buffer length in samples
        size_t writePos_{}; // The write position
        size_t loopStart_{}; // Loop start position
        size_t loopEnd_{}; // Loop end position
        size_t loopLength_{}; // Length of the loop in samples
        size_t fadeIndex_{}; // Counter used for fades
        size_t sampleRate_{}; // The sample rate
        bool feedbackPickup_{}; // True when the start position value matches the feedback position value
        bool freeze_{}; // True when the buffer is frozen
        bool forward_{}; // True if the direction is forward
        bool mustFadeIn_{}; // True if the read value must be fade in
        bool mustFadeOut_{}; // True if the read value must be fade out
        bool mustRestart_{}; // True if the read position must be restarted
        State state_{}; // The current state of the looper
        Mode mode_{}; // The currento mode of the looper
        Movement movement_{}; // The current movement type of the looper
        CrossFade cf_; // Crossfade used for fading in/out of the read value
    };
} // namespace wreath