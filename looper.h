#pragma once

#include "Dynamics/crossfade.h"
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr int kMinLoopLengthSamples{48};
    constexpr int kSamplesToFade{240}; // Note: 240 samples is 5ms @ 48KHz.

    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        enum Movement
        {
            FORWARD,
            BACKWARDS,
            PENDULUM,
            DRUNK,
            LAST_MOVEMENT,
        };

        enum Fade
        {
            NONE = -1,
            OUT,
            IN,
        };

        void Init(size_t sampleRate, float *mem, size_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetSpeedMult(float speed);
        // looplength_ = loopEnd_ + (bufferSamples_ - loopStart_) + 1
        void SetLoopLength(size_t length);
        void SetMovement(Movement movement);
        bool Buffer(float value);
        float Read(float pos);
        void SetWritePos(size_t pos);
        void Restart();
        void SetReadPos(float pos);
        void SetLoopStart(size_t pos);
        size_t GetRandomPosition();

        inline size_t GetBufferSamples() { return bufferSamples_; }
        inline size_t GetLoopStart() { return loopStart_; }
        inline size_t GetLoopEnd() { return loopEnd_; }
        inline size_t GetLoopLength() { return loopLength_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetReadPos() { return readPos_; }
        inline size_t GetWritePos() { return writePos_; }
        inline float GetNextReadPos() { return nextReadPos_; }
        inline float GetSpeedMult() { return speedMult_; }
        inline size_t GetSampleRateSpeed() { return sampleRateSpeed_; }
        inline Movement GetMovement() { return movement_; }
        inline bool IsForwardMovement() { return Movement::FORWARD == movement_; }
        inline bool IsBackwardsMovement() { return Movement::BACKWARDS == movement_; }
        inline bool IsPendulumMovement() { return Movement::PENDULUM == movement_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsGoingForward() { return forward_; }
        inline void Write(float value) { buffer_[writePos_] = value; }
        bool SetNextReadPos(float pos)
        {
            nextReadPos_ = pos;

            return HandlePosBoundaries(nextReadPos_);
        };
        inline void SetLoopEnd(size_t pos) { loopEnd_ = pos; };
        inline void SetForward(bool forward) { forward_ = forward; };
        inline void ToggleDirection() { forward_ = !forward_; };
        inline void ToggleWriting() { writingActive_ = !writingActive_; };
        inline void ToggleReading() { readingActive_ = !readingActive_; };
        inline void SetReading(bool active) { readingActive_ = active; };

        float temp{};

    private:
        void CalculateDeltaTime();
        void WrapPos(size_t &pos);
        void CalculateHeadsDistance();
        void HandleFade();
        void CalculateFadeSamples(size_t pos);
        void UpdateLoopEnd();
        bool HandlePosBoundaries(float &pos);
        float FindMinValPos(float pos);
        float ZeroCrossingPos(float pos);

        float *buffer_{};           // The buffer
        float bufferSeconds_{};     // Written buffer length in seconds
        float readPos_{};           // The read position
        float readPosSeconds_{};    // Read position in seconds
        float nextReadPos_{};       // Next read position
        float fadePos_{};           // Fade position
        float loopStartSeconds_{};  // Start of the loop in seconds
        float loopLengthSeconds_{}; // Length of the loop in seconds
        float speedMult_{};         // Speed multiplier
        float readSpeed_{};         // Actual read speed
        float writeSpeed_{};        // Actual write speed
        float headsDistance_{};     // Distance in samples between the reading and writing heads
        size_t maxBufferSamples_{}; // The whole buffer length in samples
        size_t bufferSamples_{};    // The written buffer length in samples
        size_t writePos_{};         // The write position
        size_t loopStart_{};        // Loop start position
        size_t loopEnd_{};          // Loop end position
        size_t loopLength_{};       // Length of the loop in samples
        int fadeIndex_{};           // Counter used for fades
        int fadeSamples_{};
        size_t sampleRate_{}; // The sample rate
        bool forward_{};      // True if the direction is forward
        bool crossPointFound_{};
        bool readingActive_{true};
        bool writingActive_{true};
        size_t sampleRateSpeed_{};
        Fade mustFade_{Fade::NONE};

        Movement movement_{}; // The current movement type of the looper
        CrossFade cf_;        // Crossfade used for fading in/out of the read value
    };
} // namespace wreath