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

#define arrayCount(a) sizeof(a)/sizeof(a[0])

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
    bool isConnected = false;
    bool isAnalog;
    float stickAverageX;
    float stickAverageY;
    union
    {
        game_button_state buttons[10];
        struct
        {
            game_button_state moveUp;
            game_button_state moveDown;
            game_button_state moveLeft;
            game_button_state moveRight;
            game_button_state action0;
            game_button_state action1;
            game_button_state action2;
            game_button_state action3;
            game_button_state leftBumper;
            game_button_state rightBumper;
            game_button_state start;
            game_button_state select;
        };
    };
};

struct game_input
{
    game_controller_input controllers[5];
};
inline game_controller_input *getController(game_input *input, int idx)
{
    assert(idx < arrayCount(input->controllers));
    return &input->controllers[idx];
}

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
