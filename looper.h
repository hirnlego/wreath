#pragma once

#include "head.h"
#include "Dynamics/crossfade.h"
#include <stddef.h>

namespace wreath
{
    using namespace daisysp;


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

        void Init(int32_t sampleRate, float *mem, int32_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetSpeedMult(float speed);
        // looplength_ = loopEnd_ + (bufferSamples_ - loopStart_) + 1
        void SetLoopLength(int32_t length);
        void SetMovement(Movement movement);
        bool Buffer(float value);
        float Read(float pos);
        void SetWritePos(int32_t pos);
        void Restart();
        void SetReadPos(float pos);
        void SetLoopStart(int32_t pos);
        int32_t GetRandomPosition();

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline int32_t GetLoopStart() { return loopStart_; }
        inline int32_t GetLoopEnd() { return loopEnd_; }
        inline int32_t GetLoopLength() { return loopLength_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }
        inline float GetPositionSeconds() { return readPosSeconds_; }
        inline float GetReadPos() { return readPos_; }
        inline int32_t GetWritePos() { return writePos_; }
        inline float GetNextReadPos() { return nextReadPos_; }
        inline float GetSpeedMult() { return speedMult_; }
        inline int32_t GetSampleRateSpeed() { return sampleRateSpeed_; }
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
        inline void SetLoopEnd(int32_t pos) { loopEnd_ = pos; };
        inline void SetForward(bool forward) { forward_ = forward; };
        inline void ToggleDirection() { forward_ = !forward_; };
        inline void ToggleWriting() { writingActive_ = !writingActive_; };
        inline void ToggleReading() { readingActive_ = !readingActive_; };
        inline void SetReading(bool active) { readingActive_ = active; };

        float temp{};

    private:
        void CalculateDeltaTime();
        void WrapPos(int32_t &pos);
        void CalculateHeadsDistance();
        void HandleFade();
        void CalculateFadeSamples(int32_t pos);
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
        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples
        int32_t writePos_{};         // The write position
        int32_t loopStart_{};        // Loop start position
        int32_t loopEnd_{};          // Loop end position
        int32_t loopLength_{};       // Length of the loop in samples
        int fadeIndex_{};           // Counter used for fades
        int fadeSamples_{};
        int32_t sampleRate_{}; // The sample rate
        bool forward_{};      // True if the direction is forward
        bool crossPointFound_{};
        bool readingActive_{true};
        bool writingActive_{true};
        int32_t sampleRateSpeed_{};
        Fade mustFade_{Fade::NONE};

        Head readHead_{Type::READ};
        Head writeHead_{Type::WRITE};

        Movement movement_{}; // The current movement type of the looper
        CrossFade cf_;        // Crossfade used for fading in/out of the read value
    };
} // namespace wreath