#include <cstring>
#include "ui.h"

using namespace wreath;

CrossFade cf;

/** The DSY_QSPI_BSS attribute places your array in QSPI memory */
float DSY_QSPI_BSS qspi_buffer[5];

struct CONFIGURATION
{
};

CONFIGURATION curent_config;

// https://forum.electro-smith.com/t/persisting-data-to-from-flash/502/27
void SaveConfig(uint32_t slot)
{
    /*
    size_t size = sizeof(wavform_ram[0]) * WAVE_LENGTH;
    // Grab physical address from pointer
    size_t address = (size_t)qspi_buffer;
    // Erase qspi and then write that wave
    hw.qspi.Erase(address, address + size);
    hw.qspi.Write(address, size, (uint8_t *)wavform_ram);
    */

    uint32_t base = 0x90000000;
    base += slot * 4096; // works only because sizeof(CONFIGURATION) < 4096
    hw.seed.qspi.Erase(base, base + sizeof(CONFIGURATION));
    hw.seed.qspi.Write(base, sizeof(CONFIGURATION), (uint8_t *)&curent_config);
}

void LoadConfig(uint32_t slot)
{
    memcpy(&curent_config, reinterpret_cast<void *>(0x90000000 + (slot * 4096)), sizeof(CONFIGURATION));
}


void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size)
{
    hw.ProcessAllControls();
    GenerateUiEvents();

    for (size_t i = 0; i < size; i++)
    {
        float dry_l{IN_L[i]};
        float dry_r{IN_R[i]};

        if (mustStopBuffering) {
            // When manually stopping buffering the two buffers end up with a
            // different number of samples, I guess because they're not stopped
            // at the same exact point in time. To resolve this problem, we get
            // the shortest buffer and "truncate" the other to the same length.
            const size_t min = fmin(loopers[0].GetBufferSamples(), loopers[1].GetBufferSamples());
            loopers[0].StopBuffering(min);
            loopers[1].StopBuffering(min);
            mustStopBuffering = false;
        }

        float wet_l{loopers[0].Process(dry_l, i)};
        float wet_r{loopers[1].Process(dry_r, i)};

        cf.SetPos(dryWet);
        OUT_L[i] = cf.Process(dry_l, wet_l);
        OUT_R[i] = cf.Process(dry_r, wet_r);
    }
}

int main(void)
{
    InitHw();
    InitLoopers();
    cf.Init(CROSSFADE_CPOW);

    hw.StartAudio(AudioCallback);

    while (1)
    {
        ProcessUi();
        UpdateControls();
        UpdateOled();
        //UpdateMenu();
    }
}
