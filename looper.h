#pragma once

#include "head.h"
#include <ctime>
#include <cstdint>

namespace wreath
{
    class Looper
    {
    public:
        Looper() {}
        ~Looper() {}

        void Init(int32_t sampleRate, float *buffer, float *buffer2, int32_t maxBufferSamples);
        void Reset();
        void ClearBuffer();
        void StopBuffering();
        void SetReadRate(float rate);
        void SetWriteRate(float rate);
        void SetLoopLength(float length);
        void SetMovement(Movement movement);
        void SetLooping(bool looping);
        void SetLoopSync(bool overdub);
        bool Buffer(float value);
        void SetReadPos(float position);
        void SetWritePos(float position);
        float Read(float input);
        void Write(float input);
        float Degrade(float input);
        void UpdateReadPos();
        void UpdateWritePos();
        void SwitchReadingHeads();
        void HandleCrossPointFade();
        void Start(bool now);
        void Stop(bool now);
        void Trigger();
        void SetLoopStart(float start);
        int32_t GetRandomPosition();
        void SetDirection(Direction direction);
        void ToggleDirection();
        void SetFreeze(float amount);
        void SetDegradation(float amount);
        void SetReading(bool active) { readingActive_ = active; }
        void SetWriting(bool active) { writingActive_ = active; }

        void SetSamplesToFade(float samples);

        inline float GetSamplesToFade() { return readHeads_[activeReadHead_].GetSamplesToFade(); }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetBufferSeconds() { return bufferSeconds_; }

        inline float GetLoopStart() { return loopStart_; }
        inline float GetLoopStartSeconds() { return loopStartSeconds_; }

        inline float GetLoopEnd() { return loopEnd_; }

        inline float GetLoopLength() { return loopLength_; }
        inline float GetLoopLengthSeconds() { return loopLengthSeconds_; }

        inline float GetReadPos() { return readPos_; }
        inline float GetReadPosSeconds() { return readPosSeconds_; }

        inline float GetFreeze() { return freeze_; }

        inline float GetWritePos() { return writePos_; }

        inline float GetReadRate() { return readRate_; }
        inline float GetWriteRate() { return writeRate_; }
        inline int32_t GetSampleRateSpeed() { return sampleRateSpeed_; }

        inline Movement GetMovement() { return movement_; }
        inline Direction GetDirection() { return direction_; }
        inline bool IsDrunkMovement() { return Movement::DRUNK == movement_; }
        inline bool IsGoingForward() { return Direction::FORWARD == direction_; }

        inline float GetHeadsDistance() { return headsDistance_; }
        inline float GetCrossPoint() { return crossPoint_; }
        inline bool CrossPointFound() { return crossPointFound_; }
        float CalculateDistance(float a, float b, float aSpeed, float bSpeed, Direction direction);

        bool IsReading() { return readingActive_; }
        bool IsWriting() { return writingActive_; }

    private:
        enum Fade
        {
            NO_FADE,
            FADE_IN,
            FADE_OUT,
            FADE_OUT_IN,
            FADE_TRIGGER,
        };

        void CalculateCrossPoint();

        float *buffer_{};           // The buffer
        float *freezeBuffer_{};           // The buffer
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
        float writePos_{};         // The write position
        float loopStart_{};        // Loop start position
        float loopEnd_{};          // Loop end position
        float loopLength_{};       // Length of the loop in samples
        int32_t intLoopLength_{};
        int32_t intLoopStart_{};        // Loop start position
        int32_t intLoopEnd_{};          // Loop end position
        float headsDistance_{};
        int32_t sampleRate_{}; // The sample rate
        Direction direction_{};
        float freeze_{};
        float degradation_{};
        int32_t sampleRateSpeed_{};
        bool looping_{};
        bool loopSync_{};
        bool mustSyncHeads_{};
        float crossPoint_{};
        bool crossPointFound_{};
        bool readingActive_{true};
        bool writingActive_{true};
        float lengthFadePos_{};
        bool loopChanged_{};
        bool loopLengthGrown_{};

        float eRand_{};

        Head writeHead_{Type::WRITE};
        Head readHeads_[2]{{Type::READ}, {Type::READ}};

        short activeReadHead_{};

        Fader loopFade;
        Fader triggerFade;
        Fader headsCrossFade;
        Fader loopLengthFade;
        Fader frozenFade;
        Fader startFade;
        Fader stopFade;

        Movement movement_{}; // The current movement type of the looper
    };
} // namespace wreath