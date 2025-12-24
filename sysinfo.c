#include <gem.h>
#include <stdio.h>
#include <tos.h>

#define COOKIE(a, b, c, d) \
    ((long)(a) << 24 | (long)(b) << 16 | (long)(c) << 8 | (long)(d))

static int get_cookie(long tag, long *value) {
    volatile long *jar = *(volatile long **)0x5a0;

    if (jar == 0) {
        return 0;
    }

    while (jar[0] != 0) {
        if (jar[0] == tag) {
            if (value != 0) {
                *value = jar[1];
            }
            return 1;
        }
        jar += 2;
    }

    return 0;
}

static const char *cpu_name(long cpu) {
    switch (cpu & 0xff) {
    case 0x00:
        return "68000";
    case 0x10:
        return "68010";
    case 0x20:
        return "68020";
    case 0x30:
        return "68030";
    case 0x40:
        return "68040";
    case 0x60:
        return "68060";
    default:
        return "Unknown";
    }
}

static const char *machine_name(long mch) {
    switch (mch & 0xffff) {
    case 0x0000:
        return "Atari ST";
    case 0x0001:
        return "Atari STE";
    case 0x0002:
        return "Atari TT";
    case 0x0003:
        return "Atari Falcon";
    default:
        return "Unknown";
    }
}

static const char *video_name(long vdo) {
    switch (vdo & 0xffff) {
    case 0x0000:
        return "ST Video";
    case 0x0001:
        return "STE Video";
    case 0x0002:
        return "TT Video";
    case 0x0003:
        return "Falcon Video";
    default:
        return "Unknown";
    }
}

static void print_drive_info(long drive_mask) {
    int drive;

    printf("Unidades disponibles:\n");

    for (drive = 0; drive < 32; drive++) {
        if (drive_mask & (1L << drive)) {
            struct _diskfree info;
            long result = Dfree(&info, drive + 1);

            if (result == 0) {
                unsigned long total = info.b_total * info.b_secsize * info.b_clsiz;
                unsigned long free = info.b_free * info.b_secsize * info.b_clsiz;

                printf("  %c: total %lu bytes, libre %lu bytes\n",
                       'A' + drive, total, free);
            } else {
                printf("  %c: no se pudo leer la info (error %ld)\n",
                       'A' + drive, result);
            }
        }
    }
}

static void wait_for_exit(void) {
    int message[8];
    int mx = 0;
    int my = 0;
    int mb = 0;
    int ks = 0;
    int key = 0;
    int br = 0;
    int exit_requested = 0;

    if (appl_init() < 0) {
        (void)Cconws("Pulsa ENTER para salir...\r\n");
        (void)Cconin();
        return;
    }

    printf("\nPulsa ENTER o haz clic con el raton para salir...\n");

    while (!exit_requested) {
        int events = evnt_multi(MU_KEYBD | MU_BUTTON,
                                1, 1, 1,
                                0, 0, 0, 0, 0,
                                0, 0, 0, 0, 0,
                                message, &mx, &my, &mb, &ks, &key, &br);

        if ((events & MU_BUTTON) && (mb & 1)) {
            exit_requested = 1;
        } else if (events & MU_KEYBD) {
            if ((key & 0xff) == '\r') {
                exit_requested = 1;
            }
        }
    }

    appl_exit();
}

int main(void) {
    long mem_free;
    int tosver;
    long drives;
    long value;

    mem_free = Malloc(-1);
    tosver = Sversion();
    drives = Drvmap();

    printf("Sysinfo Atari ST (Falcon 030 esperado)\n");
    printf("Memoria libre: %ld bytes\n", mem_free);
    printf("TOS: %d.%02d\n", (tosver >> 8) & 0xff, tosver & 0xff);

    if (get_cookie(COOKIE('_', 'C', 'P', 'U'), &value)) {
        printf("CPU: %s (0x%08lx)\n", cpu_name(value), value);
    } else {
        printf("CPU: no disponible (cookie _CPU)\n");
    }

    if (get_cookie(COOKIE('_', 'M', 'C', 'H'), &value)) {
        printf("Maquina: %s (0x%08lx)\n", machine_name(value), value);
    } else {
        printf("Maquina: no disponible (cookie _MCH)\n");
    }

    if (get_cookie(COOKIE('_', 'V', 'D', 'O'), &value)) {
        printf("Video: %s (0x%08lx)\n", video_name(value), value);
    } else {
        printf("Video: no disponible (cookie _VDO)\n");
    }

    if (get_cookie(COOKIE('_', 'A', 'K', 'P'), &value)) {
        printf("Teclado/Idioma (_AKP): 0x%08lx\n", value);
    } else {
        printf("Teclado/Idioma: no disponible (cookie _AKP)\n");
    }

    print_drive_info(drives);
    wait_for_exit();

    return 0;
}
