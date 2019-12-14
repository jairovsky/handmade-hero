#include "handmade.h"

inline uint32_t
safeTruncateUint64(uint64_t val)
{
    assert(val <= UINT_MAX);
    return (uint32_t)val;
}

static void
gameOutputSound(game_sound_buffer *buf, int toneHz)
{
    local_persist float tSine;
    int16_t toneVolume = 3000;
    int wavePeriod = buf->samplesPerSec / toneHz;
    int16_t *sampleOut = buf->samples;
    for (int i = 0; i < buf->sampleCount; ++i)
    {
        float sineValue = sinf(tSine);
        int16_t sampleValue = (int16_t)(sineValue * toneVolume);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
        tSine += 2.0f * PI / (float)wavePeriod;
    }
}

static void
renderWeirdGradient(game_offscreen_buffer *buf, int blueOffset, int greenOffset)
{
    uint8_t *row = (uint8_t *)buf->memory;
    for (int y = 0; y < buf->height; ++y)
    {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < buf->width; ++x)
        {
            /*
              pixel in memory:  00 00 00 00 (four hexadecimal values: B G R padding)
            */
            uint8_t b = (uint8_t)(x + blueOffset);
            uint8_t g = (uint8_t)(y + greenOffset);
            *pixel++ = ((g << 8) | b);
        }
        row += buf->pitch;
    }
}

internal void
gameUpdateAndRender(game_memory *memory,
                    game_input *input,
                    game_offscreen_buffer *videoBuf,
                    game_sound_buffer *soundBuf)
{
    /* NOTE(jairo): ensures that the 'buttons' array has size equal to
       the number of fields in the union struct */
    assert((&input->controllers[0].__struct_end - &input->controllers[0].buttons[0]) ==
           arrayCount(input->controllers[0].buttons));
    // NOTE(jairo): ensures we have enough memory to store game state
    assert(sizeof(game_state) <= memory->permStorageSize);
    game_state *gameState = (game_state *)memory->permStorage;
    if (!memory->isInitialized)
    {
        char *filename = __FILE__;
        debug_read_file_result file = DEBUGplatformReadFile(filename);
        if (file.content)
        {
            DEBUGplatformWriteFile("test.out", file.size, file.content);
            DEBUGplatformFreeFile(file.content);
        }
        gameState->greenOffset = 0;
        gameState->blueOffset = 0;
        gameState->toneHz = 256;
        memory->isInitialized = true;
    }
    for (int ctrlIdx = 0; ctrlIdx < arrayCount(input->controllers); ctrlIdx++)
    {
        game_controller_input *input0 = getController(input, ctrlIdx);
        if (input0->isConnected)
        {
            if (input0->isAnalog)
            {
                gameState->toneHz = 256 + (int)(128.0f * input0->stickAverageY);
                // gameState->blueOffset += (int)(4.0f * input0->stickAverageX);
            }
            if (input0->moveDown.endedDown)
            {
                gameState->greenOffset -= 1;
            }
            if (input0->moveUp.endedDown)
            {
                gameState->greenOffset += 1;
            }
            if (input0->moveLeft.endedDown)
            {
                gameState->blueOffset += 1;
            }
            if (input0->moveRight.endedDown)
            {
                gameState->blueOffset -= 1;
            }
        }
    }
    gameOutputSound(soundBuf, gameState->toneHz);
    renderWeirdGradient(videoBuf, gameState->blueOffset, gameState->greenOffset);
}
