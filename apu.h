#ifndef APU_H
#define APU_H
#include <stdint.h>

inline float radToDeg(float rads)
{
    return rads * 180.f / 3.14159265359f;
}

struct Channel
{
    uint8_t volume;
    uint8_t volumeSweep;
    uint8_t length;
    int32_t lengthTimer;
    int16_t freq;
    bool restart;
    bool uselength;
    bool volumeDirection;
    uint32_t envelopeTimer;
};

namespace APU
{
    const float dutyCycles[] = { .125, .25, .50, .75 };
    const uint32_t AMPLITUDE = 3500;
    const uint32_t FREQUENCY = 44100;

    extern Channel channel[4];
    extern bool channelEnabled[4];

    extern float duty1, duty2;

    /* Channel 1 frequency sweep variables */
    extern uint32_t freqSweepTimer;
    extern uint8_t freqSweepTime;
    extern bool freqSweepDirection;
    extern uint8_t freqSweepShift;
    extern int16_t frequencyShadow;

    /* Noise data */
    extern uint8_t shiftClockFreq;
    extern bool counterStepWidth;
    extern uint8_t divRatio;
    extern uint16_t lfsr;
    extern uint32_t noiseFreqTimer;
    extern uint32_t noiseScaler;

    /* Output control */
    extern bool playwave;
    extern bool poweron;
    extern float solevel_1, solevel_2;

    inline float square(float x, float duty)
    {
        x = (int)radToDeg(x) % 360;
        if (x >= 360.f * duty) return 1;
        else return -1;
    }

    inline float customWave(float x, uint8_t* data)
    {
        x = (int)radToDeg(x) % 360;
        uint32_t pos = (x / 360.f) * 32;
        int8_t val = (data[pos / 2] >> ((pos % 2) * 4)) & 0xF;
        return (val - 8) / 8.f;
    }

    void init();
    void step();

    void calcFreqSweep();
}

#endif // APU_H
