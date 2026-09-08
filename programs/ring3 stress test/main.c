#include "redux_api.h"
#include <stdint.h>

int main(redux_api_t *api)
{
    api->clear();

    api->println("=== Redux API Stress Test ===");

    api->print("print: ");
    api->println("WORKS");

    api->print("putchar: ");
    api->putchar('R');
    api->putchar('E');
    api->putchar('D');
    api->putchar('U');
    api->putchar('X');
    api->putchar('\n');

    api->println("println: WORKS");

    api->println("Starting put_pixel gradient...");

    for (uint32_t y = 0; y < 480; y++)
    {
        for (uint32_t x = 0; x < 640; x++)
        {
            uint32_t r = (x * 255) / 639;
            uint32_t g = (y * 255) / 479;
            uint32_t b = 128;

            uint32_t color =
                (r << 16) |
                (g << 8) |
                b;

            api->put_pixel(x, y, color);
        }
    }

    return 0;
}