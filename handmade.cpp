#include "handmade.h"

#if HANDMADE_NO_SOUND
void gameOutputSound(game_state *state, game_sound_buffer *buf)
{
    int16_t *sampleOutput = buf->samples;
    for (int i = 0; i < buf->sampleCount; ++i)
    {
        *sampleOutput++ = 0;
        *sampleOutput++ = 0;
    }
}
#else
void gameOutputSound(game_state *state, game_sound_buffer *buf)
{
    int tone = buf->samplesPerSec / 768;
    local_persist int period = tone;
    local_persist int mult = 1;
    int16_t *sampleOutput = buf->samples;
    int16_t sampleValue = 10000 * (int16_t)mult;
    for (int i = 0; i < buf->sampleCount; ++i)
    {
        if (period-- == 0)
        {
            period = tone;
            mult = mult * -1;
            sampleValue = 10000 * (int16_t)mult;
        }
        *sampleOutput++ = sampleValue;
        *sampleOutput++ = sampleValue;
    }
}
#endif
void renderWeirdGradient(game_offscreen_buffer *buf, int blueOffset, int greenOffset)
{
    uint8_t *row = (uint8_t *)buf->memory;
    for (int y = 0; y < buf->height; ++y)
    {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = 0; x < buf->width; ++x)
        {
            /*
              pixel in memory:  00 00 00 00 (four hexadecimal values: B G R padding)
              but we actually write 0x <padding> <R> <G> <B>
            */
            if ((x + (uint8_t)blueOffset) % 255 == 0 || (y + (uint8_t)greenOffset) % 255 == 0)
            {
                *pixel++ = 0x00ff80ff;
            }
            else
            {
                *pixel++ = 0;
            }
        }
        row += buf->pitch;
    }
}

internal void
drawRectangle(game_offscreen_buffer *videoBuf,
              float left,
              float top,
              float right,
              float bottom,
              uint32_t color)
{
    int iLeft = (int)round(left);
    int iTop = (int)round(top);
    int iRight = (int)round(right);
    int iBottom = (int)round(bottom);

    if (iLeft < 0)
    {
        iLeft = 0;
    }
    if (iTop < 0)
    {
        iTop = 0;
    }
    if (iRight > videoBuf->width)
    {
        iRight = videoBuf->width;
    }
    if (iBottom > videoBuf->height)
    {
        iBottom = videoBuf->height;
    }

    uint8_t *row = (uint8_t *)videoBuf->memory +
                   iLeft * videoBuf->bytesPerPixel +
                   iTop * videoBuf->pitch;

    for (int y = iTop; y < iBottom; y++)
    {
        uint32_t *pixel = (uint32_t *)row;
        for (int x = iLeft; x < iRight; x++)
        {
            *pixel = color;
            pixel++;
        }

        row += videoBuf->pitch;
    }
}

extern "C" HANDMADE_API GAME_UPDATE_AND_RENDER(gameUpdateAndRender)
{
    /* ensures that the 'buttons' array has size equal to
       the number of fields in the union struct */
    assert((&input->controllers[0].__struct_end - &input->controllers[0].buttons[0]) ==
           arrayCount(input->controllers[0].buttons));
    // ensures we have enough memory to store game state
    assert(sizeof(game_state) <= memory->permStorageSize);

    game_state *gameState = (game_state *)memory->permStorage;

    if (!memory->isInitialized)
    {
        memory->isInitialized = true;
    }

    for (int ctrlIdx = 0; ctrlIdx < 2; ctrlIdx++)
    {
        game_controller_input *input0 = getController(input, ctrlIdx);
    }

    drawRectangle(videoBuf, 10, 10, 970, 600, 0x00ff00ff);
}

extern "C" HANDMADE_API GAME_GET_SOUND_SAMPLES(gameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permStorage;
    gameOutputSound(gameState, soundBuf);
}
