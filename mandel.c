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

#define WIDTH    320
#define HEIGHT   200

/* LUT de 256 entradas, en formato RGB 5-6-5 */
static uint16_t mcga_lut[256];

uint16_t *screen = NULL;

/* Convierte componentes MCGA de 0..63 a 16 bits 5-6-5 */
static uint16_t rgb6_to_565(uint8_t r6, uint8_t g6, uint8_t b6)
{
    uint8_t r5 = (uint8_t)(r6 >> 1);     /* 6->5 bits */
    uint8_t g6c = g6;                    /* 6 bits ya */
    uint8_t b5 = (uint8_t)(b6 >> 1);

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
  uint32_t offset = (uint32_t)y * WIDTH + (uint32_t)x;
  screen[offset] = mcga_lut[color_index];
}

static uint16_t point565 (uint16_t x, uint16_t y)
{
  uint32_t offset = (uint32_t)y * WIDTH + (uint32_t)x;
  return screen[offset];
}

static void plot565 (uint16_t x, uint16_t y, uint16_t c)
{
  uint32_t offset = (uint32_t)y * WIDTH + (uint32_t)x;
  screen[offset] = c;
}  

static uint16_t tiempo_escape (uint16_t x, uint16_t y)
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

static void mandel_r (uint16_t xm, uint16_t ym, uint16_t w, uint16_t h)
{
  if (w <= 8 || h <= 8)
  {
    for (int y=ym; y<ym+h; y++)
    {
      for (int x=xm; x<xm+w; x++)
      {
        plot565 (x, y, tiempo_escape (x, y));
      }
    }
    return;
  }
  
  uint32_t cref = point565 (xm, ym);

  for (int x=xm; x<xm+w; x++)
  {
    uint16_t c = tiempo_escape (x, ym);
    plot565 (x, ym, c);
    if (cref != c)
      goto particionar;
    c = tiempo_escape (x, ym+h-1);
    plot565 (x, ym+h-1, c);
    if (cref != c)
      goto particionar;
  }

  for (int y=ym; y<ym+h; y++)
  {
    uint16_t c = tiempo_escape (xm, y);
    plot565 (xm, y, c);
    if (cref != c)
      goto particionar;
    c = tiempo_escape (xm+w-1, y);
    plot565 (xm+w-1, y, c);
    if (cref != c)
      goto particionar;
  }
  
  for (int y=ym+1; y<ym+h-1; y++)
    for (int x=xm+1; x<xm+w-1; x++)
      plot565 (x, y, cref);
  return;
        
particionar:
  mandel_r (xm, ym, w/2, h/2);
  mandel_r (xm + w/2, ym, w-w/2, h/2);
  mandel_r (xm, ym + h/2, w/2, h-h/2);
  mandel_r (xm + w/2, ym + h/2, w-w/2, h-h/2);
}

static void mandel_recursivo (void)
{
  mandel_r (0, 0, WIDTH, HEIGHT);
}

static void mandel (void)
{
  uint16_t x, y;
  
  for (y=0; y<HEIGHT; y++)
  {
    for (x=0; x<WIDTH; x++)
    {
      plot565 (x, y, tiempo_escape (x, y));
    }
  }
}

/* Programa principal */
int main(void)
{
    /* Estado antiguo para restaurar */
    int16_t  old_mode;
    int16_t  old_rez;
    void    *old_log;
    void    *old_phys;

    /* Nuevo modo 320x200x16bpp chunky */
    int16_t  monitor_bits;
    int16_t  freq_bits;
    int16_t  new_mode;
    long     new_size;
    long     i;

    old_mode = VsetMode(-1);      /* solo consultar */
    old_rez  = Getrez();
    old_log  = Logbase();
    old_phys = Physbase();

    /* Conservar tipo de monitor (VGA/TV) y frecuencia actual */
    monitor_bits = (int16_t)(old_mode & VGA);
    freq_bits    = (int16_t)(old_mode & PAL);

    /* Modo Falcon: 320x200 truecolor, 40 columnas, sin ST compat */
    new_mode = (int16_t)(
        (old_mode & ~(STMODES | BPS1 | BPS2 | BPS4 | BPS8)) |
        COL40 |
        BPS16 |
        monitor_bits |
        freq_bits
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

    /* Inicializar LUT MCGA -> RGB565 */
    init_mcga_lut();

    memset (screen, 0, new_size);

    mandel_recursivo(); 

    Cconin();

    /* Restaurar modo y pantalla originales */
    VsetMode(old_mode);
    Setscreen(old_log, old_phys, old_rez);

    Mfree(screen);

    return 0;
}
