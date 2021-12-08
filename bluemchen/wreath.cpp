#include "wreath.h"
#include "ui.h"
#include <cstring>

using namespace wreath;

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
    UpdateClock();
    UpdateControls();
    GenerateUiEvents();

    for (size_t i = 0; i < size; i++)
    {
        float leftIn{IN_L[i]};
        float rightIn{IN_R[i]};

        float leftOut{};
        float rightOut{};
        looper.Process(leftIn, rightIn, leftOut, rightOut);

        OUT_L[i] = leftOut;
        OUT_R[i] = rightOut;
    }
}

int main(void)
{
    InitHw(0.05f, 0.f);

    looper.Init(hw.AudioSampleRate());

    //hw.SetAudioBlockSize(24);

    hw.StartAudio(AudioCallback);

    while (1)
    {
        ProcessUi();
    }
}
