/**
 * Inspired by Monome softcut's subhead class:
 * https://github.com/monome/softcut-lib/blob/main/softcut-lib/src/SubHead.cpp
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace wreath
{
    constexpr int kMinLoopLengthSamples{48};
    constexpr int kMinLoopLengthForFade{4800};
    constexpr float kMinSamplesForTone{1440}; // 30ms @ 48KHz
    constexpr int32_t kSamplesToFade{600};

    enum Type
    {
        READ,
        WRITE,
    };

    enum Action
    {
        NO_ACTION,
        LOOP,
        INVERT,
        STOP,
    };

    enum Movement
    {
        NORMAL,
        PENDULUM,
        DRUNK,
    };

    enum Direction
    {
        BACKWARDS = -1,
        FORWARD = 1
    };

    enum RunStatus
    {
        STOPPED,
        RUNNING,
        STOPPING,
        STARTING,
        RESTARTING,
    };

    class Head
    {
    private:
        const Type type_;
        float *buffer_;

        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples

        int32_t intIndex_{};
        float index_{};
        float rate_{};
        float fadeIndex_{};

        int32_t loopStart_{};
        int32_t loopEnd_{};
        int32_t loopLength_{};

        bool looping_{};
        float snapshotValue_{};
        bool switchAndRamp_{};
        RunStatus runStatus_{};

        Movement movement_{};
        Direction direction_{};

        float currentValue_{};
        float writeBalance_{}; // Balance between new and old value when writing

        Action HandleLoopAction()
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (Direction::FORWARD == direction_)
                {
                    if (looping_ && intIndex_ > loopEnd_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            SetIndex(loopEnd_ - (index_ - loopEnd_));

                            return Action::INVERT;
                        }
                        else
                        {
                            SetIndex((loopStart_ + (index_ - loopEnd_)) - 1);

                            return Action::LOOP;
                        }
                    }
                    // When the head is not looping, and while it's not already
                    // stopping, stop it and allow for a fade out.
                    else if (RunStatus::STOPPING != runStatus_ && !looping_ && intIndex_ > loopEnd_ - SamplesToFade())
                    {
                        return Action::STOP;
                    }
                }
                // Backwards direction.
                else {
                    if (looping_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            SetIndex(loopStart_ + (loopStart_ - index_));

                            return Action::INVERT;
                        }
                        else
                        {
                            SetIndex((loopEnd_ - std::abs(loopStart_ - index_)) + 1);

                            return Action::LOOP;
                        }
                    }
                    // When the head is not looping, and while it's not already
                    // stopping, stop it and allow for a fade out.
                    else if (!looping_ && intIndex_ < loopStart_ + SamplesToFade())
                    {
                        if (RunStatus::STOPPING == runStatus_)
                        {
                            SetIndex((loopEnd_ - std::abs(loopStart_ - index_)) + 1);

                            return Action::LOOP;
                        }

                        return Action::STOP;
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                float frame = bufferSamples_ - 1;
                if (Direction::FORWARD == direction_)
                {
                    if (intIndex_ > frame)
                    {
                        // Wrap-around.
                        SetIndex((index_ - frame) - 1);

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (index_ - loopEnd_), 0.f));

                            return Action::INVERT;
                        }
                        else
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (index_ - loopEnd_) - 1, frame));

                            return looping_ ? Action::LOOP : Action::STOP;
                        }
                    }
                }
                else
                {
                    if (intIndex_ < 0)
                    {
                        // Wrap-around.
                        SetIndex((frame - std::abs(index_)) + 1);

                        return looping_ ? Action::LOOP : Action::STOP;
                    }
                    else if (intIndex_ > loopEnd_ && intIndex_ < loopStart_)
                    {
                        if (Movement::PENDULUM == movement_ && looping_)
                        {
                            // Min to avoid overflow.
                            SetIndex(std::min(loopStart_ + (loopStart_ - index_), frame));

                            return Action::INVERT;
                        }
                        else
                        {
                            // Max to avoid overflow.
                            SetIndex(std::max(loopEnd_ - (loopStart_ - index_) + 1, 0.f));

                            return looping_ ? Action::LOOP : Action::STOP;
                        }
                    }
                }
            }

            return Action::NO_ACTION;
        }

        int32_t WrapIndex(int32_t index)
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (index > loopEnd_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopEnd_ - (index - loopEnd_);
                    }
                    else
                    {
                        index = (FORWARD == direction_) ? (loopStart_ + (index - loopEnd_)) - 1 : 0;
                    }
                }
                // Backwards direction.
                else if (index < loopStart_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopStart_ + (loopStart_ - index);
                    }
                    else
                    {
                        index = (BACKWARDS == direction_) ? (loopEnd_ - std::abs(loopStart_ - index)) + 1 : 0;
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                int32_t frame{bufferSamples_ - 1};
                if (index > frame)
                {
                    index = (index - frame) - 1;
                }
                else if (index < 0)
                {
                    // Wrap-around.
                    index = (frame - std::abs(index)) + 1;
                }
                else if (index > loopEnd_ && index < loopStart_)
                {
                    if (FORWARD == direction_)
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::max(loopEnd_ - (index - loopEnd_), static_cast<int32_t>(0)) : std::min(loopStart_ + (index - loopEnd_) - 1, frame);
                    }
                    else
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::min(loopStart_ + (loopStart_ - index), frame) : std::max(loopEnd_ - (loopStart_ - index) + 1, static_cast<int32_t>(0));
                    }
                }
            }

            return index;
        }

        /*
        int32_t FindMinValPos(float pos)
        {
            float min{};
            int32_t intIndex = static_cast<int32_t>(std::floor(pos));
            int32_t minPos{intIndex};
            float value{buffer_[intIndex]};
            for (int i = 0; i < 10; i++)
            {
                intIndex = WrapIndex(intIndex + i * direction_);
                float val = buffer_[intIndex];
                if (std::abs(value - val) < min)
                {
                    min = val;
                    minPos = intIndex;
                }
            }

            return minPos;
        }

        int32_t ZeroCrossingPos(float pos)
        {
            bool sign1, sign2;
            int32_t currentPos = static_cast<int32_t>(std::floor(pos));
            int32_t pos1{};
            int32_t pos2{};
            for (int32_t i = 0; i < 10; i++)
            {
                pos1 = WrapIndex(currentPos - i);
                sign1 = buffer_[pos1] > 0;
                pos2 = WrapIndex(currentPos - i - 1);
                sign2 = buffer_[pos2] > 0;

                if (sign1 != sign2)
                {
                    return pos1;
                }

                pos1 = WrapIndex(currentPos + i);
                sign1 = buffer_[pos1] > 0;
                pos2 = WrapIndex(currentPos + i + 1);
                sign2 = buffer_[pos2] > 0;

                if (sign1 != sign2)
                {
                    return pos1;
                }
            }

            return currentPos;
        }
        */

        void CalculateLoopEnd()
        {
            if (loopStart_ + loopLength_ > bufferSamples_)
            {
                loopEnd_ = (loopStart_ + loopLength_) - bufferSamples_ - 1;
            }
            else
            {
                loopEnd_ = loopStart_ + loopLength_ - 1;
            }
        }

    public:
        Head(Type type) : type_{type} {}
        ~Head() {}

        void Reset()
        {
            intIndex_ = 0;
            index_ = 0.f;
            loopStart_ = 0;
            loopEnd_ = 0;
        }

        void Init(float *buffer, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            maxBufferSamples_ = maxBufferSamples;
            rate_ = 1.f;
            looping_ = false;
            runStatus_ = RunStatus::STOPPED;
            movement_ = Movement::NORMAL;
            direction_ = Direction::FORWARD;
            Reset();
        }

        int32_t SetLoopStart(int32_t start)
        {
            loopStart_ = std::min(std::max(start, static_cast<int32_t>(0)), bufferSamples_ - 1);
            CalculateLoopEnd();
            if (!looping_)
            {
                ResetPosition();
            }

            return loopStart_;
        }

        int32_t SetLoopLength(int32_t length)
        {
            loopLength_ = std::min(std::max(length, static_cast<int32_t>(kMinLoopLengthSamples)), bufferSamples_);
            CalculateLoopEnd();

            return loopLength_;
        }

        int32_t SamplesToFade()
        {
            return std::min(kSamplesToFade, loopLength_);
        }

        inline void SetWriteBalance(float amount) { writeBalance_ = amount; }
        inline void SetRate(float rate) { rate_ = std::abs(rate); }
        inline void SetMovement(Movement movement) { movement_ = movement; }
        inline void SetDirection(Direction direction) { direction_ = direction; }
        inline void SetIndex(float index)
        {
            index_ = index;
            intIndex_ = static_cast<int32_t>(std::floor(index));
        }
        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_ ? loopStart_ : loopEnd_);
        }
        float UpdatePosition()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                //return index_;
            }

            float index = index_ + (rate_ * direction_);
            SetIndex(index);
            Action action = HandleLoopAction();

            if (READ == type_)
            {
                switch (action)
                {
                case STOP:
                    Stop();
                    break;

                case INVERT:
                    ToggleDirection();
                    break;

                case LOOP:
                    break;

                default:
                    break;
                }
            }

            return index_;
        }

        bool IsFading()
        {
            return switchAndRamp_;
        }

        void SwitchAndRamp()
        {
            snapshotValue_ = currentValue_;//ReadAt(index_);
            switchAndRamp_ = true;
            fadeIndex_ = 0;
        }

        float Read()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return 0.f;
            }

            float value = ReadAt(index_);
            int32_t samplesToFade = SamplesToFade();

            // Gradually start reading, fading from zero to the buffered value.
            if (RunStatus::STARTING == runStatus_)
            {
                if (fadeIndex_ < samplesToFade - 1)
                {
                    float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade));
                    value *= win;
                    fadeIndex_ += 1;
                }
                else
                {
                    runStatus_ = RunStatus::RUNNING;
                }
            }
            // Gradually stop reading, fading from the buffered value to zero.
            else if (RunStatus::STOPPING == runStatus_)
            {
                if (fadeIndex_ < samplesToFade - 1)
                {
                    float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade));
                    value *= 1.0f - win;
                    fadeIndex_ += 1;
                }
                else
                {
                    value = 0.f;
                    runStatus_ = RunStatus::STOPPED;
                }
            }
            else
            {
                if (!switchAndRamp_ && std::abs(currentValue_ - value) > 0.2f)
                {
                    //SwitchAndRamp();
                }

                // Apply switch-and-ramp technique to smooth the read value.
                // http://msp.ucsd.edu/techniques/v0.11/book-html/node63.html
                if (switchAndRamp_)
                {
                    if (fadeIndex_ < samplesToFade - 1)
                    {
                        float delta = snapshotValue_ - value;
                        float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade));
                        value += delta * (1.0f - win);
                        fadeIndex_ += 1;
                    }
                    else
                    {
                        switchAndRamp_ = false;
                    }
                }
            }

            currentValue_ = value;

            return currentValue_;
        }

        float ReadAt(float index)
        {
            int32_t intPos = static_cast<int32_t>(std::floor(index));
            float value = buffer_[intPos];
            float frac = index - intPos;

            // Interpolate value only it the index has a fractional part.
            if (frac > std::numeric_limits<float>::epsilon())
            {
                float value2 = buffer_[WrapIndex(intPos + direction_)];

                return value + (value2 - value) * frac;
            }

            return value;
        }

        void Write(float value)
        {
            currentValue_ = ReadAt(index_);
            //value = value * (1.f - writeBalance_) + currentValue_ * writeBalance_;
            int32_t samplesToFade = SamplesToFade();

            // Gradually start writing, fading from the buffered value to the input
            // signal.
            if (RunStatus::STARTING == runStatus_)
            {
                float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade));
                value = currentValue_ * (1.0f - win) + value * win;
                fadeIndex_ += 1;
                if (fadeIndex_ > samplesToFade)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
            }
            // Gradually stop writing, fading from the input signal to the buffered
            // value.
            else if (RunStatus::STOPPING == runStatus_)
            {
                float win = std::sin(1.570796326794897 * fadeIndex_ * (1.f / samplesToFade));
                value = value * (1.0f - win) + currentValue_ * win;
                fadeIndex_ += 1;
                if (fadeIndex_ > samplesToFade)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
            }

            if (RunStatus::STOPPED != runStatus_)
            {
                buffer_[WrapIndex(intIndex_)] = value;
            }
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            if (intIndex_ < maxBufferSamples_ - kSamplesToFade)
            {
                buffer_[intIndex_] = value;
            }
            // Fade out recording until reaching the end of the available buffer.
            else
            {
                // Handle end of buffer.
                if (intIndex_ > maxBufferSamples_ - 1)
                {
                    return true;
                }

                int32_t d = std::abs(maxBufferSamples_ - intIndex_ - kSamplesToFade);
                buffer_[intIndex_] = value * (1.f - d / static_cast<float>(kSamplesToFade));
            }

            intIndex_++;
            bufferSamples_ = intIndex_;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1;
        }

        int32_t StopBuffering()
        {
            intIndex_ = 0;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1;
            ResetPosition();

            return bufferSamples_;
        }

        inline Direction ToggleDirection()
        {
            direction_ = static_cast<Direction>(direction_ * -1);

            return direction_;
        }

        inline void SetRunStatus(RunStatus status)
        {
            runStatus_ = status;
            fadeIndex_ = 0;
        }

        RunStatus Start()
        {
            // Start right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > kMinSamplesForTone ? RunStatus::STARTING : RunStatus::RUNNING;
            SetRunStatus(status);

            return status;
        }

        RunStatus Stop()
        {
            // Stop right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > kMinSamplesForTone ? RunStatus::STOPPING : RunStatus::STOPPED;
            SetRunStatus(status);

            return status;
        }

        inline void SetLooping(bool looping)
        {
            looping_ = looping;
        }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline int32_t GetLoopEnd() { return loopEnd_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }
        bool IsRunning() { return RunStatus::RUNNING == runStatus_; }
        RunStatus GetRunStatus() { return runStatus_; }
    };
}