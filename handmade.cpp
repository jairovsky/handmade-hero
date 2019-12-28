#include "handmade.h"

void gameOutputSound(game_sound_buffer *buf, int toneHz, float *tSine)
{
    int16_t toneVolume = 3000;
    int wavePeriod = buf->samplesPerSec / toneHz;
    int16_t *sampleOut = buf->samples;
    for (int i = 0; i < buf->sampleCount; ++i)
    {
        float sineValue = sinf(*tSine);
        int16_t sampleValue = (int16_t)(sineValue * toneVolume);
        *sampleOut++ = sampleValue;
        *sampleOut++ = sampleValue;
        *tSine += 2.0f * PI / (float)wavePeriod;
        if (*tSine > 2 * PI)
        {
            *tSine -= 2 * PI;
        }
    }
}

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
renderPlayer(game_offscreen_buffer *videoBuf, int playerX, int playerY)
{
    uint8_t *endOfBuffer = (uint8_t *)videoBuf->memory +
                           videoBuf->bytesPerPixel * videoBuf->width +
                           videoBuf->pitch * (videoBuf->height-1);
    for (int x = playerX; x < playerX + 10; x++)
    {
        uint8_t *pixel = ((uint8_t *)videoBuf->memory) +
                         x * videoBuf->bytesPerPixel +
                         playerY * videoBuf->pitch;

        for (int y = playerY; y < playerY + 10; y++)
        {
            if (pixel >= videoBuf->memory && pixel < endOfBuffer)
            {
                *(uint32_t *)pixel = 0x00ffffff;
                pixel += videoBuf->pitch;
            }
        }
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
        char *filename = __FILE__;
        debug_read_file_result file = memory->DEBUGplatformReadFile(filename);
        if (file.content)
        {
            memory->DEBUGplatformWriteFile("test.out", file.size, file.content);
            memory->DEBUGplatformFreeFile(file.content);
        }

        gameState->greenOffset = 0;
        gameState->blueOffset = 0;
        gameState->toneHz = 256;
        gameState->playerX = 100;
        gameState->playerY = videoBuf->height/2;
        gameState->tJump = -1.0f;

        memory->isInitialized = true;
    }

    for (int ctrlIdx = 0; ctrlIdx < 2; ctrlIdx++)
    {
        game_controller_input *input0 = getController(input, ctrlIdx);
        if (input0->isConnected)
        {
            if (input0->isAnalog)
            {
                gameState->toneHz = 256 + (int)(128.0f * input0->stickAverageY);
            }
            else
            {
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

            gameState->playerX += (int)(input0->stickAverageX * 4);
            if (input0->action1.endedDown && gameState->tJump < -0.99f)
            {
                gameState->tJump = 1.0f;
            }
            if (gameState->tJump > -1.0f)
            {
                gameState->playerY -= (int)(input0->stickAverageY * 4 + sinf(gameState->tJump) * 5);
                gameState->tJump -= 0.033f;
            }
        }
    }

    renderWeirdGradient(videoBuf, gameState->blueOffset, gameState->greenOffset);
    renderPlayer(videoBuf, gameState->playerX, gameState->playerY);
}

extern "C" HANDMADE_API GAME_GET_SOUND_SAMPLES(gameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permStorage;
    gameOutputSound(soundBuf, gameState->toneHz, &gameState->tSine);
}
