#pragma once

#include "head.h"
#include "Dynamics/crossfade.h"
#include <stdint.h>

namespace wreath
{
    using namespace daisysp;

    class Looper
    {

    public:
        Looper() {}
        ~Looper() {}

        enum Fade
        {
            NONE = -1,
            OUT,
            IN,
        };

        void Init(int32_t sampleRate, float *buffer, int32_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetReadRate(float rate);
        void SetLoopLength(int32_t length);
        void SetMovement(Movement movement);
        bool Buffer(float value);
        float Read();
        void Write(float value);
        void UpdateReadPos();
        void UpdateWritePos();
        void Restart();
        void SetLoopStart(int32_t pos);
        int32_t GetRandomPosition();
        void SetLoopEnd(int32_t pos);
        void SetDirection(Direction direction);
        void ToggleDirection();
        void ToggleWriting();
        void ToggleReading();

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }

        inline int32_t GetLoopStart() { return loopStart_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }

        inline int32_t GetLoopEnd() { return loopEnd_; }

        inline int32_t GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }

        inline float GetReadPos() { return readPos_; }
        inline float GetReadPosSeconds() { return readPosSeconds_; }
        inline float GetNextReadPos() { return nextReadPos_; }

        inline int32_t GetWritePos() { return writePos_; }

        inline float GetReadRate() { return readRate_; }
        inline int32_t GetSampleRateSpeed() { return sampleRateSpeed_; }

        inline Movement GetMovement() { return movement_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsGoingForward() { return Direction::FORWARD == direction_; }

        inline void SetReading(bool active) { readingActive_ = active; }
        void SetReadPosition(float position)
        {
            heads_[READ].SetIndex(position);
            readPos_ = position;
        }

        float temp{};
        int32_t CalculateHeadsDistance();
        int32_t CalculateCrossPoint();

    private:
        void CalculateDeltaTime();
        void WrapPos(int32_t &pos);
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
        float readRate_{};         // Speed multiplier
        float writeRate_{};         // Speed multiplier
        float readSpeed_{};         // Actual read speed
        float writeSpeed_{};        // Actual write speed
        int32_t headsDistance_{};     // Distance in samples between the reading and writing heads
        int32_t bufferSamples_{};    // The written buffer length in samples
        int32_t writePos_{};         // The write position
        int32_t loopStart_{};        // Loop start position
        int32_t loopEnd_{};          // Loop end position
        int32_t loopLength_{};       // Length of the loop in samples
        int fadeIndex_{};           // Counter used for fades
        int fadeSamples_{};
        int32_t sampleRate_{}; // The sample rate
        Direction direction_{};
        bool crossPointFound_{};
        bool readingActive_{true};
        bool writingActive_{true};
        int32_t sampleRateSpeed_{};
        Fade mustFade_{Fade::NONE};

        Head heads_[2]{{Type::READ}, {Type::WRITE}};

        Movement movement_{}; // The current movement type of the looper
        CrossFade cf_;        // Crossfade used for fading in/out of the read value
    };
} // namespace wreath