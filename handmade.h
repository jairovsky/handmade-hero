#ifndef HANDMADE_H

#define PI 3.14159265359f

struct game_sound_buffer {
    int      samplesPerSec;
    int      sampleCount;
    int16_t *samples;
};

struct game_offscreen_buffer {
    void* memory;
    int   width;
    int   height;
    int   pitch;
};


static void gameOutputSound(game_sound_buffer *buf);
static void gameUpdateAndRender(game_offscreen_buffer *buf, int blueOffset, int greenOffset);


#define HANDMADE_H
#endif
