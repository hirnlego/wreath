/**
 * Inspired by Monome softcut's subhead class:
 * https://github.com/monome/softcut-lib/blob/main/softcut-lib/src/SubHead.cpp
 */

#pragma once

#include <stdint.h>
#include <algorithm>

namespace wreath
{
    constexpr int kMinLoopLengthSamples{48};
    constexpr int kSamplesToFade{240}; // Note: 240 samples is 5ms @ 48KHz.

    enum Type
    {
        READ,
        WRITE,
     };

    enum Direction
    {
        BACKWARDS = -1,
        FORWARD = 1
    };

    class Head
    {
    private:
        const Type type_;
        float *buffer_;

        int32_t maxBufferSamples_{}; // The whole buffer length in samples
        int32_t bufferSamples_{};    // The written buffer length in samples

        float index_{};
        float rate_{};

        int32_t loopStart_{};
        int32_t loopEnd_{};
        int32_t loopLength_{};

        int fadeIndex_{};
        int fadeSamples_{};

        bool run_{};

        Direction direction_{};

        static inline float Hermite(float x, float y0, float y1, float y2, float y3)
        {
            return (((0.5f * (y3 - y0) + 1.5f * (y1 - y2)) * x + (y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3)) * x + 0.5f * (y2 - y0)) * x + y1;
        }

        float WrapIndex(float index)
        {
            // Handle normal loop boundaries.
            if (loopEnd_ > loopStart_)
            {
                // Forward direction.
                if (FORWARD == direction_ && index > loopEnd_)
                {
                    index = loopStart_ + (index - loopEnd_) - 1;
                    // Invert direction when in pendulum.
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopEnd_ - index;
                    }
                }
                // Backwards direction.
                else if (BACKWARDS == direction_ && index < loopStart_)
                {
                    index = loopEnd_ - (loopStart_ - index) + 1;
                    // Invert direction when in pendulum.
                    if (Movement::PENDULUM == movement_)
                    {
                        index = loopStart_ + std::abs(index);
                    }
                }
            }
            // Handle inverted loop boundaries (end point comes before start point).
            else
            {
                if (FORWARD == direction_)
                {
                    if (index > bufferSamples_ - 1)
                    {
                        // Wrap-around.
                        index = index - bufferSamples_;
                    }
                    else if (index > loopEnd_ && index < loopStart_)
                    {
                        index = loopStart_;
                        // Invert direction when in pendulum.
                        if (Movement::PENDULUM == movement_)
                        {
                            index = loopEnd_;
                        }
                    }
                }
                else
                {
                    if (index < 0)
                    {
                        // Wrap-around.
                        index = (bufferSamples_ - 1) + index;
                    }
                    else if (index > loopEnd_ && index < loopStart_)
                    {
                        index = loopEnd_;
                        // Invert direction when in pendulum.
                        if (Movement::PENDULUM == movement_)
                        {
                            index = loopStart_;
                        }
                    }
                }
            }

            return index;
        }

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
        Head(Type type): type_{type} {}
        ~Head() {}

        void Init(float *buffer, int32_t maxBufferSamples)
        {
            buffer_ = buffer;
            maxBufferSamples_ = maxBufferSamples;
        }

        void Reset()
        {
            index_ = 0.f;
            rate_ = 1.f;
            loopStart_ = 0;
            loopEnd_ = 0;
            loopLength_ = 0;
            direction_ = FORWARD;
        }

        void SetLoopStart(int32_t start)
        {
            loopStart_ = std::min(std::max(start, static_cast<int32_t>(0)), bufferSamples_ - 1);
            CalculateLoopEnd();
        }

        void SetLoopLength(int32_t length)
        {
            loopLength_ = std::min(std::max(length, static_cast<int32_t>(kMinLoopLengthSamples)), bufferSamples_);
            CalculateLoopEnd();
        }

        void SetRate(float rate)
        {
            rate_ = rate;
        }

        void SetDirection(Direction direction)
        {
            direction_ = direction;
        }

        void UpdatePosition()
        {
            index_ = WrapIndex(index_ + rate_);
        }

        float Read()
        {
            int32_t phase1 = static_cast<int32_t>(index_);
            int32_t phase0 = phase1 - 1;
            int32_t phase2 = phase1 + 1;
            int32_t phase3 = phase1 + 2;

            float y0 = buffer_[WrapIndex(phase0)];
            float y1 = buffer_[WrapIndex(phase1)];
            float y2 = buffer_[WrapIndex(phase2)];
            float y3 = buffer_[WrapIndex(phase3)];

            float x = index_ - phase1;

            float value = Hermite(x, y0, y1, y2, y3);

            // 2) fade the value if needed.
            if (mustFade_ != -1)
            {
                cf_.SetPos(fadeIndex_ * (1.f / fadeSamples_));
                float fadeValues[2][2]{{val, 0.f}, {0.f, val}};
                val = cf_.Process(fadeValues[mustFade_][0], fadeValues[mustFade_][1]);
                fadeIndex_ += 1;
                // End and reset the fade when done.
                if (fadeIndex_ > fadeSamples_)
                {
                    fadeIndex_ = 0;
                    cf_.SetPos(0.f);
                    // After a fade out there's always a fade in.
                    if (Fade::OUT == mustFade_)
                    {
                        fadeIndex_ = 0;
                        //fadePos_ = writePos_;
                        mustFade_ = Fade::IN;
                        val = 0;
                    }
                    else
                    {
                        crossPointFound_ = false;
                        mustFade_ = Fade::NONE;
                    }
                }
            }
        }

        void Write(float value)
        {

        }

        bool Buffer(float value)
        {
            // Handle end of buffer.
            if (index_ > maxBufferSamples_ - 1)
            {
                return true;
            }

            buffer_[index_++] = value;
            bufferSamples_ = index_;

            return false;
        }

        void InitBuffer(int32_t bufferSamples)
        {
            bufferSamples_ = bufferSamples;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1 ;
        }

        int32_t StopBuffering()
        {
            index_ = 0;
            loopLength_ = bufferSamples_;
            loopEnd_ = loopLength_ - 1 ;
            ResetPosition();

            return bufferSamples_;
        }

        void ResetPosition()
        {
            index_ = (FORWARD == direction_ )? loopStart_ : loopEnd_);
        }

        void ToggleDirection()
        {
            direction_ = static_cast<Direction>(direction_ * -1);
        }

        int32_t GetBufferSamples()
        {
            return bufferSamples_;
        }

        float GetRate()
        {
            return rate_;
        }

        float GetPosition()
        {
            return index_;
        }

        bool IsGoingForward()
        {
            return FORWARD == direction_;
        }
    };
}