#include <tos.h>

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

static int mode_to_bpp(int mode) {
    switch (mode & 0x07) {
    case 0:
        return 1;
    case 1:
        return 2;
    case 2:
        return 4;
    case 3:
        return 8;
    case 4:
        return 16;
    default:
        return 0;
    }
}

static int get_screen_geometry(int *width_out, int *height_out, int *bpp_out) {
    int mode = VsetMode(-1);
    long line_bytes = VsetLine(-1);
    long screen_bytes = VgetSize(-1);
    int bpp = mode_to_bpp(mode);

    if (bpp <= 0 || line_bytes <= 0 || screen_bytes <= 0) {
        return 0;
    }

    *bpp_out = bpp;
    *width_out = (int)((line_bytes * 8L) / bpp);
    *height_out = (int)(screen_bytes / line_bytes);
    return 1;
}

int main(void) {
    unsigned char *screen;
    int i;
    int x;
    int y;
    unsigned char color;
    int height;
    int width;
    int bpp;

    if (!get_screen_geometry(&width, &height, &bpp)) {
        (void)Cconws("No se pudo determinar la geometria de pantalla.\r\n");
        return 1;
    }

    (void)Cconws("Modo actual detectado:\r\n");
    Cconout('0' + (width / 100) % 10);
    Cconout('0' + (width / 10) % 10);
    Cconout('0' + (width % 10));
    (void)Cconws("x");
    Cconout('0' + (height / 100) % 10);
    Cconout('0' + (height / 10) % 10);
    Cconout('0' + (height % 10));
    (void)Cconws(" ");
    Cconout('0' + (bpp / 10) % 10);
    Cconout('0' + (bpp % 10));
    (void)Cconws("bpp\r\n");

    if (bpp != 4) {
        (void)Cconws("Este demo requiere 16 colores (4 bpp).\r\n");
        (void)Cconws("Cambia a un modo de 16 colores y vuelve a ejecutar.\r\n");
        return 1;
    }

    set_gray_palette();

    screen = (unsigned char *)Physbase();

    for (i = 0; i < 20000; i++) {
        x = (int)(Random() % width);
        y = (int)(Random() % height);
        color = (unsigned char)(Random() & 0x0F);
        plot_pixel(screen, width, x, y, color);
    }

    (void)Cconws("Pulsa una tecla para salir...\r\n");
    (void)Cconin();

    return 0;
}
