#ifndef CHECKBOX_H
#define CHECKBOX_H
#include "component.h"

class CheckBox : public Component
{
public:
    bool* check = nullptr;

    CheckBox(int32_t x, int32_t y);

    void (*oncheck)(Debugger*);

    void update(Debugger*);
    void draw(Debugger*);
};

#endif // CHECKBOX_H
