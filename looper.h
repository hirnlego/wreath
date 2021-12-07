#pragma once

#include "Dynamics/crossfade.h"
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;

    constexpr int kMinSamples{48};
    constexpr int kFadeSamples{480}; // Note: 480 samples is 10ms @ 48KHz.
    constexpr float kMinSpeed{-4.f};
    constexpr float kMaxSpeed{4.f};

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
            RANDOM,
            LAST_MOVEMENT,
        };

        void Init(size_t sampleRate, float *mem, int maxBufferSeconds);
        void ResetBuffer();
        void StopBuffering();
        void SetSpeed(float speed);
        void SetLoopLength(size_t length);
        void ResetLoopLength();
        void SetMovement(Movement movement);
        bool Buffer(float value);
        float Read(float pos);
        void SetWritePos(float pos);
        void HandlePosBoundaries(float &pos, bool isReadPos);
        void Restart();
        void SetReadPos(float pos);
        size_t GetRandomPosition();

        inline size_t GetBufferSamples() { return bufferSamples_; }
        inline size_t GetLoopStart() { return loopStart_; }
        inline size_t GetLoopEnd() { return loopEnd_; }
        inline size_t GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetReadPos() { return readPos_; }
        inline float GetWritePos() { return writePos_; }
        inline float GetNextReadPos() { return nextReadPos_; }
        inline float GetSpeed() { return speed_; }
        inline Movement GetMovement() { return movement_; }
        inline bool IsForwardMovement() { return Movement::FORWARD == movement_; }
        inline bool IsBackwardsMovement() { return Movement::BACKWARDS == movement_; }
        inline bool IsPendulumMovement() { return Movement::PENDULUM == movement_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsRandomMovement() { return Movement::RANDOM == movement_; }
        inline bool IsGoingForward() { return forward_; }
        inline void Write(float value) { buffer_[writePos_] = value; }
        inline void SetNextReadPos(float pos) { nextReadPos_ = pos; };
        inline void SetLoopStart(size_t pos) { loopStart_ = pos; };
        inline void SetLoopEnd(size_t pos) { loopEnd_ = pos; };
        inline void SetForward(bool forward) { forward_ = forward; };
        inline void ToggleDirection() { forward_ = !forward_; };
    private:
        void SetReadPosAtStart();
        void SetReadPosAtEnd();

        inline int GetFadeSamples() { return (loopLength_ > kFadeSamples * 2) ? kFadeSamples : 0; }

        float *buffer_{}; // The buffer
        float bufferSeconds_{}; // Written buffer length in seconds
        float readPos_{}; // The read position
        float readPosSeconds_{}; // Read position in seconds
        float nextReadPos_{}; // Next read position
        float fadePos_{}; // Fade position
        float loopLengthSeconds_{}; // Length of the loop in seconds
        float speed_{}; // Speed multiplier
        size_t initBufferSamples_{}; // The whole buffer length in samples
        size_t bufferSamples_{}; // The written buffer length in samples
        size_t writePos_{}; // The write position
        size_t loopStart_{}; // Loop start position
        size_t loopEnd_{}; // Loop end position
        size_t loopLength_{}; // Length of the loop in samples
        size_t fadeIndex_{}; // Counter used for fades
        size_t sampleRate_{}; // The sample rate
        bool forward_{}; // True if the direction is forward
        bool mustFadeIn_{}; // True if the read value must be fade in
        bool mustFadeOut_{}; // True if the read value must be fade out

        Movement movement_{}; // The current movement type of the looper
        CrossFade cf_; // Crossfade used for fading in/out of the read value
    };
} // namespace wreath