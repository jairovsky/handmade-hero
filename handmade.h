#ifndef HANDMADE_H


#define internal      static
#define global_var    static
#define local_persist static

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

#define HANDMADE_H
#endif
