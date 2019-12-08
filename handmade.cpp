#include "handmade.h"


static void
gameOutputSound(game_sound_buffer *buf, int toneHz)
{
    local_persist float tSine;
    int16_t toneVolume = 3000;
    int wavePeriod = buf->samplesPerSec / toneHz;
    int16_t *sampleOut = buf->samples;
    for (int i = 0; i < buf->sampleCount; ++i) {
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
gameUpdateAndRender(game_input *input,
                    game_offscreen_buffer *videoBuf,
                    game_sound_buffer *soundBuf)
{
    local_persist int blueOffset = 0;
    local_persist int greenOffset = 0;
    local_persist int toneHz = 256;

    game_controller_input *input0 = &input->controllers[0];


    if (input0->isAnalog)
        {
            toneHz = 256 + (int)(128.0f * input0->endY);
            blueOffset += (int)(4.0f * input0->endX);
        }

    if (input0->down.endedDown)
        {
            greenOffset -= 1;
        }
    if (input0->up.endedDown)
        {
            greenOffset += 1;
        }
    if (input0->left.endedDown)
        {
            blueOffset += 1;
        }
    if (input0->right.endedDown)
        {
            blueOffset -= 1;
        }

    gameOutputSound(soundBuf, toneHz);
    renderWeirdGradient(videoBuf, blueOffset, greenOffset);
}

