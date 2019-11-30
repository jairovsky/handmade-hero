#ifndef HANDMADE_H

struct game_offscreen_buffer {
    void* memory;
    int width;
    int height;
    int pitch;
};


static void gameUpdateAndRender(game_offscreen_buffer *buf, int blueOffset, int greenOffset);



#define HANDMADE_H
#endif
