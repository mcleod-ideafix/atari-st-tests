#include <tos.h>
#include <stdint.h>

#define MODE_FALCON_320x240x16 (PAL | TV | COL40 | BPS4)
#define WIDTH 320
#define HEIGHT 240

static void set_gray_palette(void) {
    int i;
    short palette[16 * 3];

    for (i = 0; i < 16; i++) {
        int level = (i * 1000) / 15;
        palette[i * 3] = (short)level;
        palette[i * 3 + 1] = (short)level;
        palette[i * 3 + 2] = (short)level;
    }

    VsetRGB(0, 16, (void *)palette);
}

static void plot_pixel(unsigned char *screen, int x, int y, unsigned char color) {
    long offset = (long)y * WIDTH + x;
    long byte_index = offset >> 1;
    unsigned char value = screen[byte_index];

    if (x & 1) {
        value = (value & 0xF0) | (color & 0x0F);
    } else {
        value = (value & 0x0F) | ((color & 0x0F) << 4);
    }

    screen[byte_index] = value;
}

int main(void) {
    unsigned char *screen;
    int i;
    int x;
    int y;
    unsigned char color;
    int old_mode;

    old_mode = VsetMode(-1);
    VsetMode(MODE_FALCON_320x240x16);
    set_gray_palette();

    screen = (unsigned char *)Physbase();

    for (i = 0; i < 20000; i++) {
        x = (int)(Random() % WIDTH);
        y = (int)(Random() % HEIGHT);
        color = (unsigned char)(Random() & 0x0F);
        plot_pixel(screen, x, y, color);
    }

    (void)Cconws("Pulsa una tecla para salir...\r\n");
    (void)Cconin();

    VsetMode(old_mode);

    return 0;
}
