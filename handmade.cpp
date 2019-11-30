#include "handmade.h"


static void
renderWeirdGradient(game_offscreen_buffer *buf, int blueOffset, int greenOffset)
{
    uint8_t *row = (uint8_t*)buf->memory;
    for (int y = 0; y < buf->height; ++y) {
        uint32_t* pixel = (uint32_t*)row;
        for (int x = 0; x < buf->width; ++x) {
            /*
              pixel in memory:  00 00 00 00 (four hexadecimal values: B G R padding)
            */
            uint8_t b = (x + blueOffset);
            uint8_t g = (y + greenOffset);
            *pixel++ = ((g << 8) | b) ;
        }
        row += buf->pitch;
    }
}

static void
gameUpdateAndRender(game_offscreen_buffer *buf, int blueOffset, int greenOffset)
{
    renderWeirdGradient(buf, blueOffset, greenOffset);
}

