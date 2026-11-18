/* Harness no host: corre a mesma GUI e grava capturas PPM. */
#include "../src/os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int hs = 0;
void *hal_alloc(uint32_t n) { return calloc(1, n); }
void hal_present(int x, int y, int w, int h) { (void)x;(void)y;(void)w;(void)h; }
void hal_time(int *h, int *m, int *s) { *h = 14; *m = 37; *s = 5 + hs; }
void hal_date(int *d, int *mo, int *y) { *d = 13; *mo = 11; *y = 2026; }
uint32_t hal_ms(void) { static uint32_t t; return t += 37; }
void hal_reboot(void) { puts("reboot"); }
void hal_shutdown(void) { puts("shutdown"); }
const char *hal_cpu(void) { return "QEMU Virtual CPU version 2.5+"; }
const char *hal_video(void) { return "1024x768x32"; }

static void shot(const char *name) {
    gui_flush();
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n%d %d\n255\n", SW, SH);
    for (int i = 0; i < SW * SH; i++) { unsigned c = back[i]; fputc(c >> 16, f); fputc(c >> 8, f); fputc(c, f); }
    fclose(f);
}
static void mv(int x, int y) { static int cx = 512, cy = 384; extern void gui_mouse(int,int,int); gui_mouse(x - cx, y - cy, 0); cx = x; cy = y; }
static int bx = 512, by = 384;
static void M(int x, int y, int b) { gui_mouse(x - bx, y - by, b); bx = x; by = y; }
static void click(int x, int y) { M(x, y, 0); M(x, y, 1); M(x, y, 0); }
static void typestr(const char *s) { for (; *s; s++) gui_key((unsigned char)*s == '\r' ? '\n' : (unsigned char)*s); }

void host_disk_blank(int); void host_disk_load(const char *); void host_disk_dump(const char *);
void host_disk_foreign(void); void host_disk_none(void);
int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "save";
    if (!strcmp(mode, "save")) {
        host_disk_blank(4);
        gfx_init(1024, 768); gui_init();
        shot("/tmp/a1.ppm");                       /* dialogo: disco novo */
        gui_key('\n');
        typestr("mkdir /docs/projetos\nwrite /docs/projetos/nota.txt olá do AuroraOS\ncp /imagens/exemplo.aimg /imagens/copia.aimg\nsave\n");
        gui_open("memoria");
        shot("/tmp/a2.ppm");
        typestr("aum\n");
        gui_key('s');
        host_disk_dump("/tmp/t.aum");
    } else if (!strcmp(mode, "load")) {
        host_disk_load(argv[2]);
        gfx_init(1024, 768); gui_init();
        shot("/tmp/a3.ppm");                       /* dialogo: memoria encontrada */
        gui_key('\n');                             /* Carregar */
        gui_open("ficheiros");
        shot("/tmp/a4.ppm");
    } else if (!strcmp(mode, "foreign")) {
        host_disk_foreign();
        gfx_init(1024, 768); gui_init();
        shot("/tmp/a5.ppm");
        gui_key('\n'); typestr("save\naum\n");
        gui_power(1);                              /* nao ha disco usavel: reinicia sem dialogo */
        host_disk_dump("/tmp/foreign_after.bin");
    } else if (!strcmp(mode, "badload")) {
        host_disk_load(argv[2]);
        gfx_init(1024, 768); gui_init();
        gui_key(27);                               /* Ignorar */
        typestr("write meu.txt dados atuais\nload\n");   /* a RAM tem alteracoes: pede confirmacao */
        gui_key('\n');                             /* Carregar */
        extern const char *mem_msg(void);
        printf("msg: %s\n", mem_msg());
        typestr("ls\n");
        shot("/tmp/a7.ppm");
    } else if (!strcmp(mode, "exit")) {
        host_disk_blank(4);
        gfx_init(1024, 768); gui_init(); gui_key('\n');
        typestr("write a.txt x\n");
        gui_power(0);
        shot("/tmp/a6.ppm");                       /* dialogo de saida */
    }
    return 0;
}
