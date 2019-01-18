#ifndef COMPONENT_H
#define COMPONENT_H
#include <stdint.h>

class Debugger;

class Component
{
protected:
    int32_t x, y;
public:
    bool pressed = false;
    Component(int32_t x, int32_t y);
    virtual void update(Debugger*);
    virtual void draw(Debugger*);
};

#endif // CHECKBOX_H
