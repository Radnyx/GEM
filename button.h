#ifndef BUTTON_H
#define BUTTON_H
#include "component.h"
#include <iostream>

class Debugger;

class Button : public Component
{
private:
    std::string text;
    int32_t width, height;

public:
    Button(std::string, int32_t x, int32_t y, int32_t w, int32_t h);

    void (*onclick)(Debugger*);

    void update(Debugger*);
    void draw(Debugger*);

    void setText(std::string text);
};

#endif // BUTTON_H
