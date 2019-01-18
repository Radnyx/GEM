#include "apu.h"
#include <SDL2/SDL.h>
#include "cpu.h"
#include <iostream>
#include <stdlib.h>

namespace APU
{


    Channel channel[4];
    bool channelEnabled[4] = {true, true, true, true};
    float channelFreq[4] = {0, 0, 0, 0};
    float channelTime[4] = {0, 0, 0, 0};
    float duty1 = 0, duty2 = 0;

    uint8_t freqSweepTime = 0;
    bool freqSweepDirection = false;
    uint32_t freqSweepTimer = 0;
    uint8_t freqSweepShift = 0;

    uint8_t shiftClockFreq = 0;
    bool counterStepWidth = false;
    uint8_t divRatio = 0;
    uint16_t lfsr = 1;
    uint32_t noiseFreqTimer = 0;
    uint32_t noiseScaler = 0;

    float solevel_1 = 1, solevel_2 = 1;
    bool playwave = false;
    bool poweron = false;

    float noise()
    {
        static double time = 0;
        time += 1;
        if (channel[3].volume == 0) time = 0;

        if (time * 0.02267573696d > 1000.d / noiseScaler)
        {
            time = 0;
            bool x = (lfsr & 1) ^ ((lfsr & 2) >> 1);
            lfsr = (lfsr >> 1);
            if (counterStepWidth)
            {
                lfsr &= ~0x40;
                lfsr |= (x << 6);
            }
            else
            {
                lfsr &= ~0x4000;
                lfsr |= (x << 14);
            }
        }

        if (!(lfsr & 1)) return 1;
        else return -1;
    }

    /*
        Generate samples based on channel waves
    */
    void generateSamples(Sint16 *stream, int length)
    {

        for (int i = 0; i < 4; i ++) {
            if (channel[i].restart) {
                channelTime[i] = 0;
                channel[i].restart = false;
            }
            channelTime[i] += 1.0d;
        }

        float lt_1 = length * channelTime[0];
        float lt_2 = length * channelTime[1];
        float lt_3 = length * channelTime[2];
        float mul = M_PI / FREQUENCY;

        for(int i = 0; i < length; i+=2) {

            float wave1 = 0;
            if (channelEnabled[0]) wave1 = square(channelFreq[0] * (lt_1 + i) * mul, duty1) * (channel[0].volume / 15.f);

            float wave2 = 0;
            if (channelEnabled[1]) wave2 = square(channelFreq[1] * (lt_2 + i) * mul, duty2) * (channel[1].volume / 15.f);

            float wave3 = 0;
            if (playwave && channelEnabled[2])
                wave3 = customWave(channelFreq[2] * (lt_3 + i) * mul, &CPU::RAM[0xFF30]) * (channel[2].volume * .25f);

            float wave4 = 0;
            if (channelEnabled[3]) wave4 = noise() * (channel[3].volume  / 15.f);

            float mixL = 0;
            float mixR = 0;

            uint8_t sout = CPU::RAM[IO_NR51];
            if (sout & 0x01) mixL += wave1;
            if (sout & 0x02) mixL += wave2;
            if (sout & 0x04) mixL += wave3;
            if (sout & 0x08) mixL += wave4;
            if (sout & 0x10) mixR += wave1;
            if (sout & 0x20) mixR += wave2;
            if (sout & 0x40) mixR += wave3;
            if (sout & 0x80) mixR += wave4;

            if (!poweron || CPU::stepmode) mixL = mixR = 0;

            stream[i] = AMPLITUDE * ( mixL ) * solevel_1;
            stream[i + 1] = AMPLITUDE * ( mixR ) * solevel_2;
        }
    }

    /* SDL audio callback function */
    void audioCallback(void*, Uint8* stream, int length)
    {
        generateSamples((Sint16*) stream, length/2);
    }

    void init()
    {
        /* Initialize SDL_Audio Specifications */
        SDL_AudioSpec desiredSpec;
        desiredSpec.freq = FREQUENCY;
        desiredSpec.format = AUDIO_S16SYS;
        desiredSpec.channels = 2; // Stereo
        desiredSpec.samples = 512;
        desiredSpec.callback = audioCallback;

        SDL_AudioSpec obtainedSpec;

        SDL_OpenAudio(&desiredSpec, &obtainedSpec);

        SDL_PauseAudio(0);
    }

    void updateVolumeEnvelope(Channel* ch)
    {
        if (ch->volumeSweep == 0) return;
        if (double(ch->envelopeTimer) * (1 / 4194304.d) > (ch->volumeSweep * 0.015625d))
        {
            if (ch->volumeDirection && ch->volume < 15)
                ch->volume++;
            else if (ch->volume > 0)
                ch->volume--;

            ch->envelopeTimer = 0;
        }
    }

    void calcFreqSweep()
    {
        uint16_t last = channel[0].freq;

        if (!freqSweepDirection) {
            channel[0].freq = last + (last / (1 << freqSweepShift));

            if (channel[0].freq > 2047) {
                channel[0].freq = 0;
                CPU::RAM[IO_NR52] &= ~1;
            }
        }
        else {
            channel[0].freq = last - (last / (1 << freqSweepShift));

            if (channel[0].freq < 0) {
                channel[0].freq = 0;
            }
        }
    }

    void updateFreqSweep()
    {
        if (freqSweepTime == 0) {
            freqSweepTimer = 0;
            return;
        }

        if (float(freqSweepTimer) > (freqSweepTime * 32713.2f))
        {
            calcFreqSweep();

            CPU::RAM[IO_NR13] = channel[0].freq & 0xFF;
            CPU::RAM[IO_NR14] &= 0xF8;
            CPU::RAM[IO_NR14] |= (channel[0].freq >> 8) & 0x7;

            freqSweepTimer = 0;
        }
    }

    void clear(int ch)
    {
        CPU::RAM[IO_NR52] &= ~(1 << ch);
        switch(ch)
        {
        case 0:
            CPU::RAM[IO_NR11] &= 0xC0;
            CPU::RAM[IO_NR12] &= 0x0F;
            break;
        case 1:
            CPU::RAM[IO_NR21] &= 0xC0;
            CPU::RAM[IO_NR22] &= 0x0F;
            break;
        case 2:
            CPU::RAM[IO_NR31] = 0;
            CPU::RAM[IO_NR32] = 0;
            break;
        case 3:
            CPU::RAM[IO_NR41] &= 0xC0;
            CPU::RAM[IO_NR42] &= 0x0F;
            break;
        }
    }

    void updateSoundLength(int ch)
    {
        if (!channel[ch].uselength) {
            channel[ch].lengthTimer = 0;
            return;
        }

        if (channel[ch].lengthTimer < 0) {
            channel[ch].volume = 0;
            channel[ch].length = 0;
            channel[ch].lengthTimer = 0;
            clear(ch);
        }
    }

    void step()
    {

        updateFreqSweep();

        channelFreq[0] =  131072.f / (2048.f - channel[0].freq);
        channelFreq[1] =  131072.f / (2048.f - channel[1].freq);
        channelFreq[2] =  65536.f / (2048.f - channel[2].freq);

        //channelFreq[3] =

        updateSoundLength(0);
        updateSoundLength(1);
        updateSoundLength(2);
        updateSoundLength(3);

        updateVolumeEnvelope(&channel[0]);
        updateVolumeEnvelope(&channel[1]);
        updateVolumeEnvelope(&channel[3]);


        static uint32_t t = SDL_GetTicks();
        if (SDL_GetTicks() - t >= 16)
        {
        //    std::cout << counterStepWidth << std::endl;
           // std::cout << (4194304.d / (524288.d / r / (double)(1 << (shiftClockFreq + 1)))) << std::endl;
            t = SDL_GetTicks();
        }


    }

}
