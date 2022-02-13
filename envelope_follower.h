// From https://forum.electro-smith.com/t/audio-cuts-out-overflowing-floats/543/14

#ifndef ENV_FOLLOW_H
#define ENV_FOLLOW_H
#include <math.h>

namespace daisysp
{

class EnvFollow
{
    private:

    float avg;      //exp average of input
    float pos_sample;   //positive sample
    float sample_noDC;  //no DC sample
    float avg_env;  //average envelope
    float w;        //weighting
    float w_env;    //envelope weighting

    public:

    EnvFollow() //default constructor
    {
        avg = 0.0f;      //exp average of input
        pos_sample = 0.0f;   //positive sample
        avg_env = 0.0f;  //average envelope
        w = 0.0001f;        //weighting
        w_env = 0.0001f;    //envelope weighting
        sample_noDC = 0.0f;
    }
    ~EnvFollow() {}

    float GetEnv(float sample)
    {
        //remove average DC offset:
        avg = (w * sample) + ((1-w) * avg);
        sample_noDC = sample - avg;

        //take absolute
        pos_sample = fabsf(sample_noDC);

        //remove ripple
        avg_env = (w_env * pos_sample) + ((1-w_env) * avg_env);

        return avg_env;
    }
};

} // namespace daisysp
#endif