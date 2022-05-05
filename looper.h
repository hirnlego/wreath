#pragma once

#include "head.h"
#include <ctime>
#include <cstdint>

namespace wreath
{
    /**
     * @brief Represents the main looper, with a reading and a writing head.
     * @author Roberto Noris
     * @date Nov 2021
     */
    class Looper
    {
    public:
        Looper() {}
        ~Looper() {}

        /**
         * @brief Initializes the looper the first time.
         *
         * @param sampleRate
         * @param buffer
         * @param maxBufferSamples
         */
        void Init(int32_t sampleRate, float *buffer, float *buffer2, int32_t maxBufferSamples);
        /**
         * @brief Resets the looper when needed.
         */
        void Reset();
        void ClearBuffer();
        /**
         * @brief Writes the given value in the buffer during the buffering procedure.
         *
         * @param value
         * @return true
         * @return false
         */
        bool Buffer(float value);
        /**
         * @brief Completes the buffering procedure.
         */
        void StopBuffering();
        /**
         * @brief Starts the reading operation, either with a fade in or immediately
         * depending on the parameter.
         *
         * @param now
         */
        void StartReading(bool now);
        /**
         * @brief Stops the reading operation, either with a fade out or immediately
         * depending on the parameter.
         *
         * @param now
         */
        void StopReading(bool now);
        /**
         * @brief Starts the writing operation, either with a fade in or immediately
         * depending on the parameter.
         *
         * @param now
         */
        void StartWriting(bool now);
        /**
         * @brief Stops the writing operation, either with a fade out or immediately
         * depending on the parameter.
         *
         * @param now
         */
        void StopWriting(bool now);
        /**
         * @brief Triggers the looper playback, either mid playback or from a stopped
         * status depending on the parameter.
         *
         * @param restart
         */
        void Trigger(bool restart);
        /**
         * @brief Sets the number of samples to be used for fading.
         *
         * @param samples
         */
        void SetSamplesToFade(float samples);
        /**
         * @brief Set the loop start position, in samples.
         *
         * @param start
         */
        void SetLoopStart(float start);
        /**
         * @brief Set the loop length, in samples.
         *
         * @param length
         */
        void SetLoopLength(float length);
        /**
         * @brief Sets the reading speed, in samples.
         *
         * @param rate
         */
        void SetReadRate(float rate);
        /**
         * @brief Sets the writing speed, in samples.
         *
         * @param rate
         */
        void SetWriteRate(float rate);
        /**
         * @brief Sets the reading head movement type.
         *
         * @param movement
         */
        void SetMovement(Movement movement);
        /**
         * @brief Sets the reading head direction.
         *
         * @param direction
         */
        void SetDirection(Direction direction);
        /**
         * @brief Sets the reading position.
         *
         * @param position
         */
        void SetReadPos(float position);
        /**
         * @brief Sets the writing position.
         *
         * @param position
         */
        void SetWritePos(float position);
        /**
         * @brief Sets whether the playback is looped or not.
         *
         * @param looping
         */
        void SetLooping(bool looping);
        /**
         * @brief Sets whether the read and write loop are synched or not.
         *
         * @param loopSync
         */
        void SetLoopSync(bool loopSync);
        /**
         * @brief Reads the current value from the buffer.
         *
         * @return float
         */
        float Read();
        /**
         * @brief Writes the provided value to the buffer.
         *
         * @param input
         */
        void Write(float input);
        /**
         * @brief Applies degradation to the given signal.
         *
         * @param input
         * @return float
         */
        float Degrade(float input);
        /**
         * @brief Sets up a fade between the two reading heads.
         */
        void FadeReadingToResetPosition();
        /**
         * @brief Updates the reading position.
         */
        void UpdateReadPos();
        /**
         * @brief Updates the writing position.
         */
        void UpdateWritePos();
        /**
         * @brief Toggles the playback direction between forward and backwards.
         */
        void ToggleDirection();
        /**
         * @brief Sets the amount of freezing.
         *
         * @param amount
         */
        void SetFreeze(float amount);
        /**
         * @brief Sets the amount of degradation.
         *
         * @param amount
         */
        void SetDegradation(float amount);
        /**
         * @brief Calculates the distance between point a and b, taking into
         * account their speed and direction. This is mainly used to calculate
         * the distance between the active reading head and the writing head.
         *
         * @param a
         * @param b
         * @param aSpeed
         * @param bSpeed
         * @param direction
         * @return float
         */
        float CalculateDistance(float a, float b, float aSpeed, float bSpeed, Direction direction);

        void SetReading(bool active) { readingActive_ = active; }
        void SetWriting(bool active) { writingActive_ = active; }

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

        /**
         * @brief Calculates where in the buffer the active reading head and the
         * writing head will meet.
         */
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
        bool triggered_{};

        float eRand_{};

        Head writeHead_{Type::WRITE};
        Head readHeads_[2]{{Type::READ}, {Type::READ}};

        short activeReadHead_{};

        Fader loopFade;
        Fader triggerFade;
        Fader headsCrossFade;
        Fader loopLengthFade;
        Fader frozenFade;
        Fader startReadingFade;
        Fader stopReadingFade;
        Fader startWritingFade;
        Fader stopWritingFade;

        Movement movement_{}; // The current movement type of the looper
    };
} // namespace wreath