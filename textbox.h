#ifndef TEXTBOX_H
#define TEXTBOX_H
#include "component.h"
#include <iostream>

class TextBox : public Component
{
private:
    uint32_t width, height;

    bool hex = true;
    uint32_t limit = -1;
public:
    std::string text;

    TextBox(int32_t x, int32_t y, uint32_t w, uint32_t h);

    void (*onenter)(TextBox* self, Debugger*);
    void update(Debugger*);
    void draw(Debugger*);

    void setLimit(uint32_t lim);
    void setText(std::string text);
    void setHex(bool b);
};

#endif // TEXTBOX_H
