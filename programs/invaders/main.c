#include <stdint.h>

typedef struct
{
    uint64_t version;

    void (*print)(const char *text);
    void (*println)(const char *text);
    void (*putchar)(char character);
    void (*clear)(uint32_t color);

    void (*put_pixel)(
        uint32_t x,
        uint32_t y,
        uint32_t color
    );

    uint8_t (*get_scancode)(void);

} redux_api_t;


/* Change these if your framebuffer resolution is different. */
#define SCREEN_W 800
#define SCREEN_H 600

#define BLACK  0x000000
#define WHITE  0xFFFFFF
#define GREEN  0x00FF00
#define RED    0xFF0000
#define YELLOW 0xFFFF00

#define PLAYER_W 30
#define PLAYER_H 10

#define INVADER_ROWS 3
#define INVADER_COLS 8
#define INVADER_W 20
#define INVADER_H 12

typedef struct
{
    int x;
    int y;
    int alive;
} Invader;


/* --------------------------------------------------------- */
/* Drawing                                                   */
/* --------------------------------------------------------- */

static void rect(
    const redux_api_t *api,
    int x,
    int y,
    int w,
    int h,
    uint32_t color)
{
    for (int py = 0; py < h; py++)
    {
        for (int px = 0; px < w; px++)
        {
            int sx = x + px;
            int sy = y + py;

            if (sx >= 0 &&
                sx < SCREEN_W &&
                sy >= 0 &&
                sy < SCREEN_H)
            {
                api->put_pixel(
                    (uint32_t)sx,
                    (uint32_t)sy,
                    color
                );
            }
        }
    }
}


static void draw_player(
    const redux_api_t *api,
    int x,
    int y)
{
    rect(api, x, y + 5, PLAYER_W, 5, GREEN);
    rect(api, x + 10, y, 10, 5, GREEN);
}


static void draw_invader(
    const redux_api_t *api,
    int x,
    int y)
{
    /* Extremely sophisticated alien technology. */

    rect(api, x + 4, y,     12, 4, WHITE);
    rect(api, x,     y + 4, 20, 4, WHITE);

    rect(api, x,     y + 8, 4, 4, WHITE);
    rect(api, x + 8, y + 8, 4, 4, WHITE);
    rect(api, x + 16,y + 8, 4, 4, WHITE);
}


/* --------------------------------------------------------- */
/* Game                                                      */
/* --------------------------------------------------------- */

int main(const redux_api_t *api)
{
    Invader invaders[INVADER_ROWS][INVADER_COLS];

    int player_x = SCREEN_W / 2 - PLAYER_W / 2;
    int player_y = SCREEN_H - 40;

    int bullet_x = 0;
    int bullet_y = 0;
    int bullet_active = 0;

    int enemy_direction = 1;
    int enemy_timer = 0;

    /*
     * Create invaders.
     */
    for (int row = 0; row < INVADER_ROWS; row++)
    {
        for (int col = 0; col < INVADER_COLS; col++)
        {
            invaders[row][col].x = 100 + col * 60;
            invaders[row][col].y = 70 + row * 40;
            invaders[row][col].alive = 1;
        }
    }

    while (1)
    {
        /*
         * INPUT
         */
        uint8_t key = api->get_scancode();

        /* ESC */
        if (key == 0x01)
            return 0;

        /* Left arrow / numpad 4 */
        if (key == 0x4B)
        {
            player_x -= 8;

            if (player_x < 0)
                player_x = 0;
        }

        /* Right arrow / numpad 6 */
        if (key == 0x4D)
        {
            player_x += 8;

            if (player_x > SCREEN_W - PLAYER_W)
                player_x = SCREEN_W - PLAYER_W;
        }

        /* SPACE */
        if (key == 0x39 && !bullet_active)
        {
            bullet_x = player_x + PLAYER_W / 2;
            bullet_y = player_y - 8;
            bullet_active = 1;
        }


        /*
         * BULLET
         */
        if (bullet_active)
        {
            bullet_y -= 6;

            if (bullet_y < 0)
            {
                bullet_active = 0;
            }
        }


        /*
         * BULLET COLLISION
         */
        if (bullet_active)
        {
            for (int row = 0; row < INVADER_ROWS; row++)
            {
                for (int col = 0; col < INVADER_COLS; col++)
                {
                    Invader *enemy = &invaders[row][col];

                    if (!enemy->alive)
                        continue;

                    if (bullet_x >= enemy->x &&
                        bullet_x < enemy->x + INVADER_W &&
                        bullet_y >= enemy->y &&
                        bullet_y < enemy->y + INVADER_H)
                    {
                        enemy->alive = 0;
                        bullet_active = 0;
                    }
                }
            }
        }


        /*
         * MOVE INVADERS
         */
        enemy_timer++;

        if (enemy_timer > 20)
        {
            enemy_timer = 0;

            int hit_edge = 0;

            for (int row = 0; row < INVADER_ROWS; row++)
            {
                for (int col = 0; col < INVADER_COLS; col++)
                {
                    Invader *enemy = &invaders[row][col];

                    if (!enemy->alive)
                        continue;

                    enemy->x += enemy_direction * 4;

                    if (enemy->x <= 10 ||
                        enemy->x + INVADER_W >= SCREEN_W - 10)
                    {
                        hit_edge = 1;
                    }
                }
            }

            if (hit_edge)
            {
                enemy_direction = -enemy_direction;

                for (int row = 0; row < INVADER_ROWS; row++)
                {
                    for (int col = 0; col < INVADER_COLS; col++)
                    {
                        if (invaders[row][col].alive)
                            invaders[row][col].y += 10;
                    }
                }
            }
        }


        /*
         * DRAW
         */
        api->clear(BLACK);

        draw_player(api, player_x, player_y);

        for (int row = 0; row < INVADER_ROWS; row++)
        {
            for (int col = 0; col < INVADER_COLS; col++)
            {
                if (invaders[row][col].alive)
                {
                    draw_invader(
                        api,
                        invaders[row][col].x,
                        invaders[row][col].y
                    );
                }
            }
        }

        if (bullet_active)
        {
            rect(
                api,
                bullet_x,
                bullet_y,
                3,
                8,
                YELLOW
            );
        }
    }

    return 0;
}