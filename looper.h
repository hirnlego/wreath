#pragma once

#include "head.h"
#include <cstdint>

namespace wreath
{
    class Looper
    {
    public:
        Looper() {}
        ~Looper() {}

        void Init(int32_t sampleRate, float *buffer, int32_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetReadRate(float rate);
        void SetWriteRate(float rate);
        void SetLoopLength(float length);
        void SetMovement(Movement movement);
        void SetLooping(bool looping);
        bool Buffer(float value);
        void SetReadPos(float position);
        void SetWritePos(float position);
        float Read(float input);
        void Write(float value);
        void UpdateReadPos();
        void UpdateWritePos();
        void HandleFade();
        bool Start(bool now);
        bool Stop(bool now);
        bool Restart(bool resetPosition);
        void SetLoopStart(float start);
        int32_t GetRandomPosition();
        void SetLoopEnd(float end);
        void SetDirection(Direction direction);
        void ToggleDirection();
        void SetWriting(float amount);

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }

        inline float GetLoopStart() { return loopStart_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }

        inline float GetLoopEnd() { return loopEnd_; }

        inline float GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }

        inline float GetReadPos() { return readPos_; }
        inline float GetReadPosSeconds() { return readPosSeconds_; }

        inline int32_t GetWritePos() { return writePos_; }

        inline float GetReadRate() { return readRate_; }
        inline float GetWriteRate() { return writeRate_; }
        inline int32_t GetSampleRateSpeed() { return sampleRateSpeed_; }

        inline Movement GetMovement() { return movement_; }
        inline Direction GetDirection() { return direction_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsGoingForward() { return Direction::FORWARD == direction_; }

        inline void SetReading(bool active) { readingActive_ = active; }

        inline int32_t GetHeadsDistance() { return headsDistance_; }
        inline int32_t GetCrossPoint() { return crossPoint_; }
        inline bool CrossPointFound() { return crossPointFound_; }

    private:
        int32_t CalculateDistance(int32_t a, int32_t b, float aSpeed, float bSpeed);
        void CalculateCrossPoint();

        float *buffer_{};           // The buffer
        float bufferSeconds_{};     // Written buffer length in seconds
        float readPos_{};           // The read position
        float readPosSeconds_{};    // Read position in seconds
        float loopStartSeconds_{};  // Start of the loop in seconds
        float loopLengthSeconds_{}; // Length of the loop in seconds
        float readRate_{};         // Speed multiplier
        float writeRate_{};         // Speed multiplier
        float readSpeed_{};         // Actual read speed
        float writeSpeed_{};        // Actual write speed
        int32_t bufferSamples_{};    // The written buffer length in samples
        int32_t writePos_{};         // The write position
        float loopStart_{};        // Loop start position
        float loopEnd_{};          // Loop end position
        float loopLength_{};       // Length of the loop in samples
        int32_t intLoopLength_{};
        int32_t intLoopStart_{};        // Loop start position
        int32_t intLoopEnd_{};          // Loop end position
        int32_t headsDistance_{};
        int32_t sampleRate_{}; // The sample rate
        Direction direction_{};
        bool readingActive_{true};
        bool writingActive_{true};
        int32_t sampleRateSpeed_{};
        bool looping_{};
        bool isRestarting_{};
        bool crossPointFade_{};
        int32_t crossPoint_{};
        bool crossPointFound_{};
        float fadeBufferPos_{};
        bool mustPasteFadeBuffer_{};
        bool mustCopyFadeBuffer_{};

        float fadePos_{};
        bool mustPaste_{};

        float fadeBuffer[static_cast<int32_t>(kSamplesToFade)]{};

        Head heads_[2]{{Type::READ}, {Type::WRITE}};

        Movement movement_{}; // The current movement type of the looper
    };
} // namespace wreath