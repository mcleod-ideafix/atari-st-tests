#include <stdio.h>
#include <tos.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ---- Definiciones Falcon ---- */

#define STMODES  0x80   /* bit ST-compat, lo limpiamos para truecolor */
#define COL80    0x08
#define COL40    0x00
#define VGA      0x10
#define PAL      0x20
#define BPS1     0x00
#define BPS2     0x01
#define BPS4     0x02
#define BPS8     0x03
#define BPS16    0x04    /* 16 bpp truecolor */

#define WIDTH    640
#define HEIGHT   400

/* LUT de 256 entradas, en formato RGB 5-6-5 */
static uint16_t mcga_lut[256];

uint16_t *screen = NULL;

/* Convierte componentes MCGA de 0..63 a 16 bits 5-6-5 */
static uint16_t rgb6_to_565(uint8_t r6, uint8_t g6, uint8_t b6)
{
    uint8_t r5  = (uint8_t)(r6 >> 1);   /* 6->5 bits */
    uint8_t g6c = g6;                   /* 6 bits ya */
    uint8_t b5  = (uint8_t)(b6 >> 1);

    return (uint16_t)((r5 << 11) | (g6c << 5) | b5);
}

/* Inicializa mcga_lut[] con una aproximación de la paleta MCGA “clásica” */
static void init_mcga_lut(void)
{
    int i;

    /* 0–15: colores básicos */
    const uint8_t base[16][3] = {
        {  0,  0,  0}, {  0,  0, 42}, {  0, 42,  0}, {  0, 42, 42},
        { 42,  0,  0}, { 42,  0, 42}, { 42, 21,  0}, { 42, 42, 42},
        { 21, 21, 21}, { 21, 21, 63}, { 21, 63, 21}, { 21, 63, 63},
        { 63, 21, 21}, { 63, 21, 63}, { 63, 63, 21}, { 63, 63, 63}
    };

    for (i = 0; i < 16; i++) {
        uint8_t r = base[i][0];
        uint8_t g = base[i][1];
        uint8_t b = base[i][2];
        mcga_lut[i] = rgb6_to_565(r, g, b);
    }

    /* 16–31: escala de grises */
    for (i = 16; i < 32; i++) {
        uint8_t v = (uint8_t)((i - 16) * 4); /* 0..63 aprox */
        mcga_lut[i] = rgb6_to_565(v, v, v);
    }

    /* 32–247: cubo 6×6×6 (r,g,b en {0,12,24,36,48,63}) */
    {
        int r, g, b;
        int idx = 32;
        uint8_t v[6] = { 0, 12, 24, 36, 48, 63 };

        for (r = 0; r < 6; r++) {
            for (g = 0; g < 6; g++) {
                for (b = 0; b < 6; b++) {
                    mcga_lut[idx++] =
                        rgb6_to_565(v[r], v[g], v[b]);
                }
            }
        }
    }

    /* 248–255: últimos 8 de la tabla, tonos oscuros/grises */
    for (i = 248; i < 256; i++) {
        uint8_t v = (uint8_t)((i - 248) * 8 + 8);   /* ≈ 8..64 */
        mcga_lut[i] = rgb6_to_565(v, v, v);
    }
}

/* Escribe un pixel “chunky” 16 bpp usando índice MCGA (0..255) */
static void plot (uint16_t x, uint16_t y, uint8_t color_index)
{
    screen[(uint32_t)y * WIDTH + (uint32_t)x] = mcga_lut[color_index];
}

static uint16_t point565 (uint16_t x, uint16_t y)
{
    return screen[(uint32_t)y * WIDTH + (uint32_t)x];
}

static void plot565 (uint16_t x, uint16_t y, uint16_t c)
{
    screen[(uint32_t)y * WIDTH + (uint32_t)x] = c;
}

#define tiempo_escape tiempo_escape_fx
//#define tiempo_escape tiempo_escape_fp
static uint16_t tiempo_escape_fp (uint16_t x, uint16_t y) 
{
  float xmin = -2.4; 
  float xmax = 0.8; 
  float ymin = -1.2; 
  float ymax = 1.2; 
  float cr, ci, zr, zi, tr, ti; 
  uint8_t color; 
  
  cr = xmin + x * (xmax - xmin) / (WIDTH - 1); 
  ci = ymin + y * (ymax - ymin) / (HEIGHT - 1); 
  zr = cr; 
  zi = ci; 
  color = 0; 
  
  while (zr*zr+zi*zi<4 && color != 255) 
  { 
    color++; 
    tr = zr*zr - zi*zi + cr; 
    ti = 2.0*zr*zi + ci; 
    zr = tr; 
    zi = ti; 
  } 
  return mcga_lut[color]; 
}

static uint16_t tiempo_escape_fx (uint16_t x, uint16_t y)
{
  int64_t cr = -157286 + x*209715/(WIDTH-1);  // trabajamos con Q(16.16)
  int64_t ci = -78643 + y*157286/(HEIGHT-1);
  int64_t zr = cr;
  int64_t zi = ci;

  uint8_t cont = 0;
  while (cont != 255 && zr*zr+zi*zi<17179869184LL)
  {
    cont++;
    int64_t tr = (zr*zr-zi*zi)/65536 + cr;
    int64_t ti = (2*zr*zi)/65536 + ci;
    zr = tr;
    zi = ti;
  }
  return mcga_lut[cont];
}

static void mandel_r (uint16_t xm, uint16_t ym, uint16_t w, uint16_t h)
{
    if (w <= 4 || h <= 4) {
        uint16_t x, y;
        for (y = ym; y < ym + h; y++) {
            for (x = xm; x < xm + w; x++) {
                plot565 (x, y, tiempo_escape (x, y));
            }
        }
        return;
    }

    {
        uint16_t x, y;
        uint16_t cref = tiempo_escape (xm, ym);

        /* Pintamos bordes y comprobamos homogeneidad */
        for (x = xm; x < xm + w; x++) {
            uint16_t c;

            c = tiempo_escape (x, ym);
            plot565 (x, ym, c);
            if (cref != c)
                goto particionar;

            c = tiempo_escape (x, ym + h - 1);
            plot565 (x, ym + h - 1, c);
            if (cref != c)
                goto particionar;
        }

        for (y = ym; y < ym + h; y++) {
            uint16_t c;

            c = tiempo_escape (xm, y);
            plot565 (xm, y, c);
            if (cref != c)
                goto particionar;

            c = tiempo_escape (xm + w - 1, y);
            plot565 (xm + w - 1, y, c);
            if (cref != c)
                goto particionar;
        }

        /* Rellenar interior con el color homogéneo */
        for (y = ym + 1; y < ym + h - 1; y++) {
            uint16_t x2;
            for (x2 = xm + 1; x2 < xm + w - 1; x2++) {
                plot565 (x2, y, cref);
            }
        }
        return;

particionar:
        {
            uint16_t w2 = w / 2;
            uint16_t h2 = h / 2;
            mandel_r (xm,          ym,          w2,     h2);
            mandel_r (xm + w2,     ym,          w - w2, h2);
            mandel_r (xm,          ym + h2,     w2,     h - h2);
            mandel_r (xm + w2,     ym + h2,     w - w2, h - h2);
        }
    }
}

static void mandel_recursivo (void)
{
    mandel_r (0, 0, WIDTH, HEIGHT);
}

static void mandel (void)
{
    uint16_t x, y;

    for (y = 0; y < HEIGHT; y++) {
        for (x = 0; x < WIDTH; x++) {
            plot565 (x, y, tiempo_escape (x, y));
        }
    }
}

/* Programa principal */
int main(void)
{
    int32_t tcom, tfin;
    
    /* Estado antiguo para restaurar */
    int16_t  old_mode;
    int16_t  old_rez;
    void    *old_log;
    void    *old_phys;

    /* Nuevo modo 640x400x16bpp chunky */
    int16_t  monitor_bits;
    int16_t  freq_bits;
    int16_t  new_mode;
    long     new_size;

    old_mode = VsetMode(-1);      /* solo consultar */
    old_rez  = Getrez();
    old_log  = Logbase();
    old_phys = Physbase();

    /* Conservar tipo de monitor (VGA/TV) y frecuencia actual */
    monitor_bits = (int16_t)(old_mode & VGA);
    freq_bits    = (int16_t)(old_mode & PAL);

    /* Modo Falcon: 640x400 truecolor, 80 columnas, sin ST compat */
    new_mode = (int16_t)(
        (old_mode & ~(STMODES | BPS1 | BPS2 | BPS4 | BPS8)) |
        COL80 |
        BPS16 |
        VERTFLAG |    /* interlaced 400 líneas */
        freq_bits      /* normalmente PAL */
        /* monitor_bits será 0 (TV) si no estabas en VGA */
    );

    /* Tamaño necesario para el nuevo modo */
    new_size = VgetSize(new_mode);
    if (new_size <= 0) {
        Cconws("VgetSize() fallo\r\n");
        return 1;
    }

    /* Reservar memoria para la pantalla nueva */
    screen = (uint16_t *)Malloc(new_size);
    if (!screen) {
        Cconws("Malloc() fallo\r\n");
        return 1;
    }

    /* Mover pantalla lógica y física a nuestro buffer */
    Setscreen((void *)screen, (void *)screen, -1);

    /* Cambiar modo de vídeo */
    VsetMode(new_mode);

    /* Inicializar LUT MCGA -> RGB565 y parámetros fijos del fractal */
    init_mcga_lut();

    memset(screen, 0, new_size);

    tcom = Tickcal();
    
    /* Elegir una de las dos versiones */
    mandel_recursivo();
    /* o bien: mandel(); */

    tfin = Tickcal();
    
    printf ("Tiempo: %ld seg.", (tfin-tcom)/1000);
    Cconin();

    /* Restaurar modo y pantalla originales */
    VsetMode(old_mode);
    Setscreen(old_log, old_phys, old_rez);

    Mfree(screen);

    return 0;
}
