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
    constexpr float kMinLoopLengthSamples{46.f};  // ~C1 @ 48KHz
    constexpr float kSamplesToFade{48.f * 100}; // 20ms @ 48KHz
    constexpr float kMaxSamplesToFade{4800.f}; // 100ms @ 48KHz
    constexpr float kMinSamplesForTone{91.f}; // ~C2 @ 48KHz
    constexpr float kMinSamplesForFlanger{1722.f};
    constexpr float kSwitchAndRampThresh{0.2f};
    constexpr float kFreezeResolution{0.0001f};
    constexpr float kEqualCrossFadeP{1.25f};

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
        float *buffer2_;

        float *fadeBuffer_;
        int32_t fadeBufferIndex_{};

        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples

        int32_t intIndex_{};
        float index_{};
        float rate_{};
        float fadeIndex_{};

        float loopStart_{};
        int32_t intLoopStart_{};
        float loopEnd_{};
        int32_t intLoopEnd_{};
        float loopLength_{};
        int32_t intLoopLength_{};

        bool looping_{};
        float snapshotValue_{};
        float switchAndRampPos_{};
        bool mustSwitchAndRamp_{};
        bool switchAndRamp_{};
        RunStatus runStatus_{};

        Movement movement_{};
        Direction direction_{};

        float previousValue_{};
        float currentValue_{};
        float freezeAmount_{};
        bool frozen_{};

        float samplesToFade_{kSamplesToFade};
        float fadeRate_{1.f};

        Action HandleLoopAction()
        {
            // Handle normal loop boundaries.
            if (intLoopEnd_ > intLoopStart_)
            {
                // Forward direction.
                if (Direction::FORWARD == direction_)
                {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ < intLoopStart_)
                    {
                        //SetIndex(loopStart_);

                        return Action::NO_ACTION;
                    }
                    if (looping_ && intIndex_ > intLoopEnd_)
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
                    else if (RunStatus::RUNNING == runStatus_ && !looping_ && intIndex_ > intLoopEnd_ - samplesToFade_)
                    {
                        return Action::STOP;
                    }
                }
                // Backwards direction.
                else {
                    // This prevents "dragging" the index while changing the
                    // loop's start point.
                    if (intIndex_ > intLoopEnd_)
                    {
                        SetIndex(loopEnd_);

                        return Action::NO_ACTION;
                    }
                    if (looping_ && intIndex_ < intLoopStart_)
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
                    else if (!looping_ && intIndex_ < intLoopStart_ + samplesToFade_)
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
                    else if (intIndex_ > intLoopEnd_ && intIndex_ < intLoopStart_)
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
                    else if (intIndex_ > intLoopEnd_ && intIndex_ < intLoopStart_)
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
            if (intLoopEnd_ > intLoopStart_)
            {
                // Forward direction.
                if (index > intLoopEnd_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = intLoopEnd_ - (index - intLoopEnd_);
                    }
                    else
                    {
                        index = (FORWARD == direction_) ? (intLoopStart_ + (index - intLoopEnd_)) - 1 : 0;
                    }
                }
                // Backwards direction.
                else if (index < intLoopStart_)
                {
                    if (Movement::PENDULUM == movement_)
                    {
                        index = intLoopStart_ + (intLoopStart_ - index);
                    }
                    else
                    {
                        index = (BACKWARDS == direction_) ? (intLoopEnd_ - std::abs(intLoopStart_ - index)) + 1 : 0;
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
                else if (index > intLoopEnd_ && index < intLoopStart_)
                {
                    if (FORWARD == direction_)
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::max(intLoopEnd_ - (index - intLoopEnd_), static_cast<int32_t>(0)) : std::min(intLoopStart_ + (index - intLoopEnd_) - 1, frame);
                    }
                    else
                    {
                        // Max/min to avoid overflow.
                        index = (Movement::PENDULUM == movement_) ? std::min(intLoopStart_ + (intLoopStart_ - index), frame) : std::max(intLoopEnd_ - (intLoopStart_ - index) + 1, static_cast<int32_t>(0));
                    }
                }
            }

            return index;
        }

        void CalculateLoopEnd()
        {
            //SetSwitchAndRamp(loopEnd_);
            if (intLoopStart_ + intLoopLength_ > bufferSamples_)
            {
                loopEnd_ = (loopStart_ + loopLength_) - bufferSamples_ - 1;
            }
            else
            {
                loopEnd_ = loopStart_ + loopLength_ - 1;
            }
            intLoopEnd_ = loopEnd_;
        }

    public:
        Head(Type type) : type_{type} {}
        ~Head() {}

        void Reset()
        {
            intIndex_ = 0;
            index_ = 0.f;
            intLoopStart_ = 0;
            intLoopEnd_ = 0;
        }

        static float CrossFade(float from, float to, float pos)
        {
            float in = std::sin(pos * 1.570796326794897);
            float out = std::sin((1.f - pos) * 1.570796326794897);

            return from * out + to * in;
        }

        static float LinearCrossFade(float from, float to, float pos)
        {
            return from * (1.f - pos) + to;
        }

        /**
         * @brief Energy preserving crossfade
         * @see https://signalsmith-audio.co.uk/writing/2021/cheap-energy-crossfade/
         *
         * @param from
         * @param to
         * @param pos
         * @return float
         */
        static float EqualCrossFade(float from, float to, float pos)
        {
            float invPos = 1.f - pos;
            float k = -6.0026608f + kEqualCrossFadeP * (6.8773512f - 1.5838104f * kEqualCrossFadeP);
            float a = pos * invPos;
            float b = a * (1.f + k * a);
            float c = (b + pos);
            float d = (b + invPos);

            return from * d * d + to * c * c;
        }

        void Init(float *buffer, float *buffer2, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            buffer2_ = buffer2;
            maxBufferSamples_ = maxBufferSamples;
            rate_ = 1.f;
            looping_ = false;
            runStatus_ = RunStatus::STOPPED;
            movement_ = Movement::NORMAL;
            direction_ = Direction::FORWARD;
            Reset();
        }

        float SetLoopStart(float start)
        {
            loopStart_ = start;
            intLoopStart_ = loopStart_;
            CalculateLoopEnd();
            if (!looping_)
            {
                ResetPosition();
            }
            switchAndRamp_ = false;
            return loopStart_;
        }

        float SetLoopLength(float length)
        {
            loopLength_ = length;
            intLoopLength_ = loopLength_;
            CalculateLoopEnd();
            samplesToFade_ = std::min(samplesToFade_, loopLength_);
            switchAndRamp_ = false;

            return loopLength_;
        }

        bool mustFreeze_{};
        bool mustUnfreeze_{};
        float freezeFadeIndex_{};
        inline void SetFreeze(float amount)
        {
            freezeAmount_ = amount;
            bool frozen = amount > 0;
            if (type_ == Type::READ)
            {
                frozen_ = frozen;
            }
            else
            {
                if (frozen_ && !frozen && !mustUnfreeze_)
                {
                    // Fade in recording in the freeze buffer.
                    mustUnfreeze_ = true;
                    freezeFadeIndex_ = 0;
                }
                else if (!frozen_ && frozen && !mustFreeze_)
                {
                    // Fade out recording in the freeze buffer.
                    mustFreeze_ = true;
                    freezeFadeIndex_ = 0;
                }
            }
        }
        inline void SetRate(float rate)
        {
            rate_ = std::abs(rate);
            switchAndRamp_ = false;
        }
        inline void SetMovement(Movement movement)
        {
            movement_ = movement;
            switchAndRamp_ = false;
        }
        inline void SetDirection(Direction direction)
        {
            direction_ = direction;
            switchAndRamp_ = false;
        }
        inline void SetIndex(float index)
        {
            index_ = index;
            intIndex_ = std::floor(index_);
        }
        inline void ResetPosition()
        {
            SetIndex(FORWARD == direction_ ? loopStart_ : loopEnd_);
        }
        float UpdatePosition()
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return index_;
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

        float GetSamplesToFade()
        {
            return samplesToFade_;
        }

        void SetSamplesToFade(float samples)
        {
            samplesToFade_ = loopLength_ ? std::min(samples, loopLength_) : samples;
        }

        void SetWriteFade(float samples, float rate)
        {
            samplesToFade_ = samples;
            fadeRate_ = rate;
        }

        void SetSwitchAndRamp(float start)
        {
            switchAndRamp_ = true;
            switchAndRampPos_ = start;
            fadeIndex_ = 0;
        }

        float fadePos_{};
        bool copyFadeBuffer_{};
        void CopyFadeBuffer(float pos)
        {
            fadePos_ = pos;
            copyFadeBuffer_ = true;
            fadeBufferIndex_ = 0;
        }
        bool pasteFadeBuffer_{};
        void PasteFadeBuffer(float pos)
        {
            fadePos_ = pos;
            pasteFadeBuffer_ = true;
            fadeBufferIndex_ = 0;
        }

        void SwitchAndRamp()
        {
            snapshotValue_ = previousValue_;
            switchAndRamp_ = true;
            fadeIndex_ = 0;
            //samplesToFade_ = std::min(static_cast<int32_t>(std::abs(snapshotValue_ - currentValue_) * 1000), loopLength_);
            //samplesToFade_ = SamplesToFade();
        }

        /*
        bool toggleOnset{true};
        int32_t previousE_{};
        bool BresenhamEuclidean()
        {
            float pulses = 64.f;
            float ratio = bufferSamples_ / pulses;
            float onsets = freezeAmount_ * pulses;
            if (onsets == pulses)
            {
                toggleOnset = false;
            }
            else if (onsets == 0)
            {
                toggleOnset = true;
            }
            else
            {
                float slope = onsets / pulses;
                int32_t current = (index_ / ratio) * slope;
                if (current != previousE_)
                {
                    toggleOnset = !toggleOnset;
                }
                previousE_ = current;
            }

            return toggleOnset;
        }
        */

        bool mustFadeInFrozen{};
        float freezeLoopFadeIndex_{};
        float Read(float valueToFade)
        {
            if (RunStatus::STOPPED == runStatus_)
            {
                return 0.f;
            }

            previousValue_ = currentValue_;
            float value = ReadAt(buffer_, index_);
            currentValue_ = value;

            float frozenValue{};
            // When frozen, fade the start of the loop with the end.
            if (frozen_)
            {
                frozenValue = ReadAt(buffer2_, index_);
                if (intIndex_ == intLoopStart_ && intLoopLength_ < bufferSamples_ && intLoopLength_ >= samplesToFade_)
                {
                    mustFadeInFrozen = true;
                    freezeLoopFadeIndex_ = 0;
                }
                if (mustFadeInFrozen)
                {
                    // Read samples from past the loop end.
                    float fadeValue = ReadAt(buffer2_, WrapIndex(loopEnd_ + rate_ + freezeLoopFadeIndex_));
                    frozenValue = EqualCrossFade(fadeValue, frozenValue, freezeLoopFadeIndex_ * (1.f / samplesToFade_));
                    if (freezeLoopFadeIndex_ >= samplesToFade_)
                    {
                        mustFadeInFrozen = false;
                    }
                    freezeLoopFadeIndex_ += rate_;
                }
            }

            //valueToFade = switchAndRamp_ ? valueToFade : 0;

            // Copy some samples in the buffer used for fading.
            if (copyFadeBuffer_)
            {
                /*
                fadeBuffer_[fadeBufferIndex_] = ReadAt(buffer_, fadePos_ + fadeBufferIndex_);
                if (fadeBufferIndex_ == samplesToFade_ - 1)
                {
                    copyFadeBuffer_ = false;
                }
                fadeBufferIndex_++;
                */
            }

            // Gradually start reading, fading from the input signal to the
            // buffered value.
            if (RunStatus::STARTING == runStatus_)
            {
                value = CrossFade(valueToFade, value, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += rate_;
            }
            // Gradually stop reading, fading from the buffered value to the
            // input signal.
            else if (RunStatus::STOPPING == runStatus_)
            {
                value = CrossFade(value, valueToFade, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += rate_;
            }

            if (frozen_)
            {
                value = EqualCrossFade(value, frozenValue, freezeAmount_);
            }

/*
            // Crossfade the samples of the fade buffer.
            if (pasteFadeBuffer_)
            {
                value = CrossFade(fadeBuffer_[fadeBufferIndex_], value, fadeBufferIndex_ * (1.f / samplesToFade_));
                if (fadeBufferIndex_ == samplesToFade_ - 1)
                {
                    pasteFadeBuffer_ = false;
                }
                fadeBufferIndex_++;
            }

            if (switchAndRamp_)
            {
                float v = ReadAt(buffer_, fadePos_ + fadeBufferIndex_);
                value = CrossFade(fadeBuffer_[fadeBufferIndex_], value, fadeBufferIndex_ * (1.f / samplesToFade_));
                if (fadeBufferIndex_ == samplesToFade_ - 1)
                {
                    pasteFadeBuffer_ = false;
                }
                fadeBufferIndex_++;
            }

            if (copyFadeBuffer_)
            {
                // Read samples from past the loop end.
                //valueToFade = ReadAt(buffer_, WrapIndex(loopEnd_ + rate_ + fadeBufferIndex_));
                value = EqualCrossFade(0, value, fadeBufferIndex_ * (1.f / samplesToFade_));
                if (fadeBufferIndex_ >= samplesToFade_)
                {
                    copyFadeBuffer_ = false;
                }
                fadeBufferIndex_ += rate_;
            }

            if (intIndex_ == intLoopStart_ && intLoopLength_ < bufferSamples_ && intLoopLength_ >= samplesToFade_)
            {
                //CopyFadeBuffer(23123);
            }
*/
            return value;
        }

        float ReadAt(float *buffer, float index)
        {
            int32_t intPos = index;
            float value = buffer[intPos];
            float frac = index - intPos;

            // Interpolate value only it the index has a fractional part.
            if (frac > std::numeric_limits<float>::epsilon())
            {
                value = value + (buffer[WrapIndex(intPos + std::max(1.f, rate_ * direction_))] - value) * frac;
            }

            return value;
        }

        void HandleFreeze(float input)
        {
            float frozenValue = buffer2_[intIndex_];
            if (mustFreeze_)
            {
                input = EqualCrossFade(input, frozenValue, freezeFadeIndex_ * (1.f / samplesToFade_));
                if (freezeFadeIndex_ >= samplesToFade_)
                {
                    mustFreeze_ = false;
                    frozen_ = true;
                }
                freezeFadeIndex_ += rate_;
            }
            else if (mustUnfreeze_)
            {
                input = EqualCrossFade(frozenValue, input, freezeFadeIndex_ * (1.f / samplesToFade_));
                if (freezeFadeIndex_ >= samplesToFade_)
                {
                    mustUnfreeze_ = false;
                    frozen_ = false;
                }
                freezeFadeIndex_ += rate_;
            }
            if (!frozen_ || mustUnfreeze_)
            {
                buffer2_[intIndex_] = input;
            }
        }

        void Write(float input)
        {
            HandleFreeze(input);

            currentValue_ = buffer_[intIndex_];

            // Gradually start writing, fading from the buffered value to the input
            // signal.
            if (RunStatus::STARTING == runStatus_)
            {
                input = CrossFade(currentValue_, input, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::RUNNING;
                }
                fadeIndex_ += fadeRate_;
            }
            // Gradually stop writing, fading from the input signal to the buffered
            // value.
            else if (RunStatus::STOPPING == runStatus_)
            {
                input = CrossFade(input, currentValue_, fadeIndex_ * (1.f / samplesToFade_));
                if (fadeIndex_ >= samplesToFade_)
                {
                    runStatus_ = RunStatus::STOPPED;
                }
                fadeIndex_ += fadeRate_;
            }

            buffer_[intIndex_] = input;
        }

        void ClearBuffer()
        {
            memset(buffer_, 0.f, maxBufferSamples_);
            memset(buffer2_, 0.f, maxBufferSamples_);
        }

        bool Buffer(float value)
        {
            buffer_[intIndex_] = value;
            buffer2_[intIndex_] = value;
            bufferSamples_ = intIndex_ + 1;

            // End of available buffer?
            if (intIndex_ >= maxBufferSamples_ - 1)
            {
                return true;
            }

            intIndex_++;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            intLoopLength_ = loopLength_;
            loopEnd_ = loopLength_ - 1.f;
            intLoopEnd_ = loopEnd_;
            samplesToFade_ = std::min(samplesToFade_, loopLength_);
        }

        int32_t StopBuffering()
        {
            index_ = 0.f;
            intIndex_ = 0;
            loopLength_ = bufferSamples_;
            intLoopLength_ = loopLength_;
            loopEnd_ = loopLength_ - 1.f;
            intLoopEnd_ = loopEnd_;
            ResetPosition();
            samplesToFade_ = std::min(samplesToFade_, loopLength_);

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
            RunStatus status = loopLength_ > samplesToFade_ ? RunStatus::STARTING : RunStatus::RUNNING;
            SetRunStatus(status);

            return status;
        }

        RunStatus Stop()
        {
            // Stop right away if the loop length is smaller than the minimum
            // number of samples needed for a tone.
            RunStatus status = loopLength_ > samplesToFade_ ? RunStatus::STOPPING : RunStatus::STOPPED;
            SetRunStatus(status);

            return status;
        }

        inline void SetLooping(bool looping)
        {
            looping_ = looping;
        }

        inline int32_t GetBufferSamples() { return bufferSamples_; }
        inline float GetLoopEnd() { return loopEnd_; }
        inline float GetLoopLength() { return loopLength_; }
        inline float GetRate() { return rate_; }
        inline float GetPosition() { return index_; }
        inline int32_t GetIntPosition() { return intIndex_; }
        bool IsGoingForward() { return Direction::FORWARD == direction_; }
        bool IsRunning() { return RunStatus::RUNNING == runStatus_; }
        RunStatus GetRunStatus() { return runStatus_; }
        float GetCurrentValue() { return currentValue_; }
    };
}