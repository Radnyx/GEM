#ifndef GPU_H
#define GPU_G
#include <stdint.h>

namespace GPU
{
    extern uint32_t rendercycles;

    void init();

    void step();

    void refresh();

    uint32_t getWindowID();
    void raise();
}

#endif // GPU_H
