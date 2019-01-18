#ifndef TAS_H
#define TAS_H
#include <stdint.h>

class TAS
{
private:
    uint8_t* data = nullptr;
    bool running = false;

    uint32_t frames = 0;
    uint32_t index = 0;
    uint32_t length = 0;

    uint8_t getByte();
    uint32_t getInt();
public:
    int loadVBM(const char* file);
    void step();

    bool isRunning();
    void stop();
};

#endif // TAS_H
