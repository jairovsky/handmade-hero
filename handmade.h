#ifndef HANDMADE_H


#define internal      static
#define global_var    static
#define local_persist static

#define PI 3.14159265359f

struct game_sound_buffer
{
    int      samplesPerSec;
    int      sampleCount;
    int16_t *samples;
};

struct game_offscreen_buffer
{
    void* memory;
    int   width;
    int   height;
    int   pitch;
};

struct game_button_state
{
    int nHalfTransitions;
    bool endedDown;
};

struct game_controller_input
{
    bool isAnalog;
    float startX;
    float startY;
    float minX;
    float minY;
    float maxX;
    float maxY;
    float endX;
    float endY;
    union
    {
        game_button_state buttons[6];
        struct
        {
            game_button_state up;
            game_button_state down;
            game_button_state left;
            game_button_state right;
            game_button_state leftBumper;
            game_button_state rightBumper;
        };
    };
};

struct game_input
{
    game_controller_input controllers[4];
};



#define HANDMADE_H
#endif
