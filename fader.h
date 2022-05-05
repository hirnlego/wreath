#pragma once

#include <cmath>

namespace wreath
{
    constexpr float kSamplesToFade{48.f * 100};       // 100ms @ 48KHz
    constexpr float kSamplesToFadeTrigger{48.f * 10}; // 10ms @ 48KHz
    constexpr float kEqualCrossFadeP{1.25f};

    /**
     * @brief Handles different types of cross-fading between two sources.
     * @author Roberto Noris
     * @date Mar 2022
     */
    class Fader
    {
    public:
        Fader() {}
        ~Fader() {}

        enum class FadeType
        {
            FADE_SINGLE,
            FADE_OUT_IN,
        };

        enum class FadeStatus
        {
            CREATED,
            PENDING,
            FADING,
            ENDED,
        };

        void Reset(float samples, float rate)
        {
            samples_ = FadeType::FADE_OUT_IN == type_ ? samples / 2.f : samples;
            freq_ = 1.f / samples_;
            rate_ = rate;
            status_ = FadeStatus::PENDING;
            index_ = 0;
            toggle_ = false;
        }

        void Init(FadeType type = FadeType::FADE_SINGLE, float samples = kSamplesToFade, float rate = 1.f)
        {
            if (FadeStatus::CREATED == status_ || FadeStatus::ENDED == status_)
            {
                type_ = type;
                Reset(samples, rate);
            }
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

        FadeStatus Process(float fromInput, float toInput)
        {
            input_ = fromInput;

            if (FadeStatus::CREATED == status_)
            {
                return status_;
            }
            if (FadeStatus::ENDED == status_)
            {
                return FadeStatus::CREATED;
            }

            status_ = FadeStatus::FADING;

            float from = fromInput;
            float to = toInput;
            if (toggle_)
            {
                from = toInput;
                to = fromInput;
            }
            output_ = EqualCrossFade(from, to, index_ * freq_);
            index_ += rate_;
            if (index_ >= samples_)
            {
                if (FadeType::FADE_OUT_IN == type_)
                {
                    index_ = 0;
                    toggle_ = true;
                    type_ = FadeType::FADE_SINGLE;
                }
                else
                {
                    status_ = FadeStatus::ENDED;
                }

                return status_;
            }

            return status_;
        }

        float GetIndex()
        {
            return index_;
        }

        FadeType GetType()
        {
            return type_;
        }

        float GetOutput()
        {
            return output_;
        }

        bool IsActive()
        {
            return FadeStatus::PENDING == status_ || FadeStatus::FADING == status_;
        }

    private:
        FadeType type_{FadeType::FADE_SINGLE};
        FadeStatus status_{FadeStatus::CREATED};
        float index_{};
        float samples_{};
        float freq_{};
        float rate_{};
        float input_{};
        float output_{};
        bool toggle_{};
    };
}