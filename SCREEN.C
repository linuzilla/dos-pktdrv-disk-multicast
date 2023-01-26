#include <dos.h>
#include <conio.h>

#include "netcphd.h"


void setattr(int attr, int x, int y, int width, int height)
{
    int         i, j;
    union REGS  registers, reg;
    int         xx, yy;

    x      =  (x + 79) % 80;
    y      =  (y + 24) % 25;
    width  %= 81;
    height %= 26;

    registers.h.ah = 3;
    registers.h.bh = 0;
    int86(0x10, &registers, &registers);
    xx = registers.h.dl;
    yy = registers.h.dh;

    registers.h.bh = 0;
    registers.h.bl = attr;

    for (i = x; i < x + width; i++) {
        registers.h.dl = i;
        for (j = y; j < y + height; j++) {
            registers.h.ah = 2;
            registers.h.dh = j;
            int86(0x10, &registers, &reg);
            registers.h.ah = 8;
            int86(0x10, &registers, &reg);
            registers.h.al = reg.h.al;
            registers.h.ah = 9;
            registers.x.cx = 1;
            int86(0x10, &registers, &reg);
        }
    }
    registers.h.ah = 2;
    registers.h.dl = xx;
    registers.h.dh = yy;
    int86(0x10, &registers, &reg);
    return;
}

int do_select(const int num, const int x, const int y, const int len, const int attr, const int bar)
{
    int current = 0, newpos = 0, c;
    setattr(bar, x, y, len, 1);

    clear_keyboard_buffer();
    while (1) {
        if ((c = getch()) == 0) {
            switch(getch()) {
            case 72: // up
                newpos = (current - 1 + num) % num;
                break;
            case 80: // down
                newpos = (current + 1) % num;
                break;
            case 73: // pgup
                newpos = 0;
                break;
            case 81: // pgdn
                newpos = num - 1;
                break;
            }
            if (newpos != current) {
                setattr(bar,  x, y+newpos,  len, 1);
                setattr(attr, x, y+current, len, 1);
                current = newpos;
            }
        } else {
            switch (c) {
            case ' ' :
            case '\r':
            case '\n':
                return current+1;
            case  27 :
                return 0;
            }
        }
    }
}
