#ifndef HANDMADE_H

#include <stdint.h>
#include <limits.h>
#include <stdint.h>
#include <math.h>
#include <cstdio>

#ifdef HANDMADE_WIN32
#ifdef HANDMADE_EXPORTS
#define HANDMADE_API __declspec(dllexport)
#else
#define HANDMADE_API __declspec(dllimport)
#endif
#else
#define HANDMADE_API
#endif

#ifdef HANDMADE_SLOW_BUILD
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

#define DEBUG_PLATFORM_READ_FILE(name) debug_read_file_result name(char *filename)
typedef DEBUG_PLATFORM_READ_FILE(debug_platform_read_file);

#define DEBUG_PLATFORM_WRITE_FILE(name) bool name(char *filename, uint32_t size, void* content)
typedef DEBUG_PLATFORM_WRITE_FILE(debug_platform_write_file);

#define DEBUG_PLATFORM_FREE_FILE(name) void name(void *file)
typedef DEBUG_PLATFORM_FREE_FILE(debug_platform_free_file);
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
        game_button_state buttons[12];
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
            game_button_state __struct_end;
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
    float tSine;
};

struct game_memory
{
    bool isInitialized;
    uint64_t permStorageSize;
    void* permStorage;
    uint64_t transientStorageSize;
    void* transientStorage;

    debug_platform_read_file *DEBUGplatformReadFile;
    debug_platform_write_file *DEBUGplatformWriteFile;
    debug_platform_free_file *DEBUGplatformFreeFile;
};

#define GAME_UPDATE_AND_RENDER(name) void name(game_memory *memory, game_input *input, game_offscreen_buffer *videoBuf)
typedef GAME_UPDATE_AND_RENDER(game_update_and_render);

#define GAME_GET_SOUND_SAMPLES(name) void name(game_memory *memory, game_sound_buffer *soundBuf)
typedef GAME_GET_SOUND_SAMPLES(game_get_sound_samples);

#define HANDMADE_H
#endif
