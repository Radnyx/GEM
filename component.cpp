#include "component.h"
#include "debug.h"

Component::Component(int32_t x, int32_t y)
{
    this->x = x;
    this->y = y;
}

void Component::update(Debugger*){}
void Component::draw(Debugger*){}
