#ifndef HANDMADE_H

#ifndef HANDMADE_SLOW_BUILD
#define assert(expr) if (!(expr)) {*(int *) 0 = 0;}
#else
#define assert(expr)
#endif

#define internal      static
#define global_var    static
#define local_persist static

#define PI 3.14159265359f

#define KILOBYTE (uint64_t)1024
#define MEGABYTE (uint64_t)1024 * KILOBYTE
#define GIGABYTE (uint64_t)1024 * MEGABYTE
#define TERABYTE (uint64_t)1024 * GIGABYTE


#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    uint32_t size;
    void * content;
};
internal debug_read_file_result DEBUGplatformReadFile(char *filename);
internal void DEBUGplatformFreeFile(void *file);
internal bool DEBUGplatformWriteFile(char *filename, uint32_t size, void* content);
#endif


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

struct game_state
{
    int greenOffset;
    int blueOffset;
    int toneHz;
};

struct game_memory
{
    bool isInitialized;
    uint64_t permStorageSize;
    void* permStorage;
    uint64_t transientStorageSize;
    void* transientStorage;
};

#define HANDMADE_H
#endif
