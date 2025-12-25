#include <tos.h>

#define MODE_FALCON_320x240x16 0x0010
#define MODE_ST_320x200x16 0x0000
#define PAL_MASK 0x0002
#define WIDTH 320

static void set_gray_palette(void) {
    int i;
    short palette[16 * 3];

    for (i = 0; i < 16; i++) {
        int level = (i * 1000) / 15;
        palette[i * 3] = (short)level;
        palette[i * 3 + 1] = (short)level;
        palette[i * 3 + 2] = (short)level;
    }

    VsetRGB(0, 16, palette);
}

static void plot_pixel(unsigned char *screen, int width, int x, int y,
                       unsigned char color) {
    long offset = (long)y * width + x;
    long byte_index = offset >> 1;
    unsigned char value = screen[byte_index];

    if (x & 1) {
        value = (value & 0xF0) | (color & 0x0F);
    } else {
        value = (value & 0x0F) | ((color & 0x0F) << 4);
    }

    screen[byte_index] = value;
}

static int is_vga_monitor(int monitor_type) {
    return monitor_type == 2;
}

static int set_graphics_mode(int *height_out) {
    int monitor = VgetMonitor();
    int old_mode = VsetMode(-1);
    int pal_bits = old_mode & PAL_MASK;
    int mode = MODE_FALCON_320x240x16 | pal_bits;

    VsetMode(mode);

    if (is_vga_monitor(VgetMonitor())) {
        VsetMode(MODE_ST_320x200x16 | pal_bits);
        *height_out = 200;
        return MODE_ST_320x200x16 | pal_bits;
    }

    if (monitor != VgetMonitor()) {
        VsetMode(MODE_ST_320x200x16 | pal_bits);
        *height_out = 200;
        return MODE_ST_320x200x16 | pal_bits;
    }

    *height_out = 240;
    return mode;
}

int main(void) {
    unsigned char *screen;
    int i;
    int x;
    int y;
    unsigned char color;
    int old_mode;
    int height;

    old_mode = VsetMode(-1);
    set_graphics_mode(&height);
    set_gray_palette();

    screen = (unsigned char *)Physbase();

    for (i = 0; i < 20000; i++) {
        x = (int)(Random() % WIDTH);
        y = (int)(Random() % height);
        color = (unsigned char)(Random() & 0x0F);
        plot_pixel(screen, WIDTH, x, y, color);
    }

    (void)Cconws("Pulsa una tecla para salir...\r\n");
    (void)Cconin();

    VsetMode(old_mode);

    return 0;
}
