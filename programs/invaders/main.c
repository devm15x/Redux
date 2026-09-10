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

int main(const redux_api_t *api)
{
    api->println("RAW SCANCODE TEST");

    while (1)
    {
        uint8_t scancode = api->get_scancode();

        if (scancode == 0)
            continue;

        if (scancode == 0x01)
        {
            api->println("ESC PRESSED");
            while (1)
            {
            }
        }

        if (scancode == 0x39)
        {
            api->println("SPACE PRESSED");
        }

        if (scancode == 0x4B)
        {
            api->println("LEFT PRESSED");
        }

        if (scancode == 0x4D)
        {
            api->println("RIGHT PRESSED");
        }
    }
}