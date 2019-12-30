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
              float r, float g, float b)
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

    uint32_t color = (uint32_t)(
        (((int)round(r * 255.0f)) << 16) |
        (((int)round(g * 255.0f)) << 8) |
        (((int)round(b * 255.0f)) << 0));

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

struct tile_map {
    int width;
    int height;
    int tileWidthPx;
    int tileHeightPx;
    uint32_t * tiles;
};

#define TM_ACCESS(tm, y, x) (tm.tiles[y * tm.width + x])
#define TMPTR_ACCESS(tm, y, x) (tm->tiles[y * tm->width + x])

internal bool
pointCollision(float pointX,
               float pointY,
               tile_map *tm)
{
    int xTile = (int)pointX / tm->tileWidthPx;
    int yTile = (int)pointY / tm->tileHeightPx;

    return xTile < 0 || xTile > tm->width ||
           yTile < 0 || yTile > tm->height ||
           TMPTR_ACCESS(tm, yTile, xTile);
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

        gameState->playerX = 510;
        gameState->playerY = 50;
    }

    uint32_t tiles[9][16] = {
        {1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1},
        {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1},
        {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1}
    };
    tile_map tileMap = {};
    tileMap.tiles = (uint32_t *)tiles;
    tileMap.width = arrayCount(tiles[0]);
    tileMap.height = arrayCount(tiles);
    tileMap.tileWidthPx = videoBuf->width / tileMap.width;
    tileMap.tileHeightPx = videoBuf->height / tileMap.height;

    float playerWidth = 0.75f * tileMap.tileWidthPx;
    float playerHeight = (float)tileMap.tileHeightPx;

    for (int ctrlIdx = 0; ctrlIdx < 2; ctrlIdx++)
    {
        game_controller_input *input0 = getController(input, ctrlIdx);

        float playerSpeed = (60.0f * input->secsToComputeUpdate);
        float dPlayerX = 0.0f;
        float dPlayerY = 0.0f;
        if (input0->moveDown.endedDown)
        {
            dPlayerY += playerSpeed;
        }
        if (input0->moveUp.endedDown)
        {
            dPlayerY -= playerSpeed;
        }
        if (input0->moveLeft.endedDown)
        {
            dPlayerX -= playerSpeed;
        }
        if (input0->moveRight.endedDown)
        {
            dPlayerX += playerSpeed;
        }

        float newPlayerX = gameState->playerX + dPlayerX;
        float newPlayerY = gameState->playerY + dPlayerY;

        bool collision = pointCollision(newPlayerX - (playerWidth / 2), newPlayerY, &tileMap) ||
                         pointCollision(newPlayerX + (playerWidth / 2), newPlayerY, &tileMap) ||
                         pointCollision(newPlayerX - (playerWidth / 2), newPlayerY - (playerHeight/2), &tileMap) ||
                         pointCollision(newPlayerX + (playerWidth / 2), newPlayerY - (playerHeight/2), &tileMap);


        if (!collision)
        {
            gameState->playerX += dPlayerX;
            gameState->playerY += dPlayerY;
        }
    }

    drawRectangle(videoBuf, 10, 10, 970, 600, 0.7f, 0.7f, 0.7f);
    for (int y = 0; y < tileMap.height; y++)
    {
        for (int x = 0; x < tileMap.width; x++)
        {
            if (TM_ACCESS(tileMap, y, x))
            {
                float left = (float)(x * tileMap.tileWidthPx);
                float top = (float)(y * tileMap.tileHeightPx);
                float right = (float)(left + tileMap.tileWidthPx);
                float bottom = (float)(top + tileMap.tileHeightPx);
                drawRectangle(videoBuf, left, top, right, bottom, 0.3f, 0.3f, 0.3f);
            }
        }
    }

    float playerLeft = gameState->playerX - 0.5f * playerWidth;
    float playerTop = gameState->playerY - playerHeight;
    drawRectangle(videoBuf,
                  playerLeft,
                  playerTop,
                  playerLeft + playerWidth,
                  playerTop + playerHeight,
                  1.0f, 1.0f, 0.0f);
}

extern "C" HANDMADE_API GAME_GET_SOUND_SAMPLES(gameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permStorage;
    gameOutputSound(gameState, soundBuf);
}
