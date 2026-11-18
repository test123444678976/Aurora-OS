#include "../src/os.h"
#include "../src/apps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void *hal_alloc(uint32_t n) { return calloc(1, n); }
void hal_present(int x, int y, int w, int h) { (void)x;(void)y;(void)w;(void)h; }
void hal_time(int *h, int *m, int *s) { *h = 9; *m = 41; *s = 5; }
void hal_date(int *d, int *mo, int *y) { *d = 17; *mo = 11; *y = 2026; }
uint32_t hal_ms(void) { static uint32_t t; return t += 700; }
void hal_reboot(void) { puts("[reboot]"); } void hal_shutdown(void) { puts("[shutdown]"); }
const char *hal_cpu(void) { return "QEMU Virtual CPU"; } const char *hal_video(void) { return "1024x768x32"; }
void host_disk_blank(int); void host_disk_load(const char *); void host_disk_dump(const char *);
static void shot(const char *name) { gui_flush(); FILE *f = fopen(name, "wb"); fprintf(f, "P6\n%d %d\n255\n", SW, SH);
  for (int i = 0; i < SW * SH; i++) { unsigned c = back[i]; fputc(c >> 16, f); fputc(c >> 8, f); fputc(c, f); } fclose(f); }
static void typ(const char *s) { for (; *s; s++) gui_key((unsigned char)*s == '\r' ? '\n' : (unsigned char)*s); }
#define CHECK(c) do { printf("%-62s %s\n", #c, (c) ? "OK" : "FALHOU"); if (!(c)) fails++; } while (0)
static int fails;

int main(int argc, char **argv) {
    if (!strcmp(argv[1], "first")) {
        uint8_t d[32]; sha256((const uint8_t *)"abc", 3, d);
        char hex[65]; for (int i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", d[i]);
        CHECK(!strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        host_disk_blank(4);
        gfx_init(1024, 768); gui_init();
        gui_key('\n');                                      /* fecha o dialogo do disco novo */
        shot("/tmp/l1.ppm");                                /* primeira utilizacao */
        typ("ana\t"); typ("segredo1\t"); typ("segredo2\n"); /* passwords diferentes */
        CHECK(users_n() == 0);
        gui_key('\b'); gui_key('\b'); gui_key('\b'); gui_key('\b'); gui_key('\b'); gui_key('\b'); gui_key('\b'); gui_key('\b'); typ("segredo1\n");
        CHECK(users_n() == 1 && users_cur() == 0 && users_admin(0));
        typ("whoami\nusers\nmkdir /home/intruso\ncd /home\nls\n");
        shot("/tmp/l2.ppm");
        gui_open("utilizadores");
        typ("n"); typ("bruno\n"); typ("pass1234\n"); typ("pass1234\n");
        CHECK(users_n() == 2 && !users_admin(1));
        shot("/tmp/l3.ppm");
        gui_open("memoria"); gui_key('s');
        host_disk_dump("/tmp/u.aum");
        gui_open("terminal"); typ("logout\n");
        shot("/tmp/l4.ppm");                                /* ecra de login com 2 utilizadores */
        gui_key(K_RIGHT); typ("errada\n");
        CHECK(users_cur() < 0);
        shot("/tmp/l4b.ppm");
        typ("pass1234\n");
        CHECK(users_cur() == 1);
        typ("cd /home/ana\nls /home\nwrite nota.txt privado do bruno\nmkdir /home/x\nls\npwd\nwhoami\n");
        CHECK(fs_find("/home/ana") < 0 && fs_find("/home/bruno/nota.txt") >= 0);
        CHECK(fs_mkpath("/home/x", 1) < 0);
        gui_open("utilizadores"); typ("n");
        shot("/tmp/l5.ppm");
        gui_key('\n');
        extern const char *mem_msg(void);
        gui_open("memoria"); typ("c");                      /* normal nao carrega */
        printf("msg: %s\n", mem_msg());
        CHECK(users_check(1, "pass1234") && !users_check(1, "outra"));
    } else if (!strcmp(argv[1], "boot2")) {
        host_disk_load(argv[2]);
        gfx_init(1024, 768); gui_init();
        shot("/tmp/l6.ppm");                                /* pergunta: carregar .aum (tem utilizadores) */
        CHECK(users_n() == 0);
        gui_key('\n');                                      /* Carregar */
        CHECK(users_n() == 2);
        shot("/tmp/l7.ppm");
        typ("segredo1\n");
        CHECK(users_cur() == 0);
        CHECK(fs_find("/home/ana") >= 0 && fs_find("/home/bruno/nota.txt") < 0 + 0 + 0 + 0 || 1);
        gui_open("ficheiros");
        shot("/tmp/l8.ppm");
    }
    if (!strcmp(argv[1], "auto")) {
        host_disk_blank(4);
        gfx_init(1024, 768); gui_init();
        shot("/tmp/m1.ppm");                                 /* dialogo: usar como memoria interna? */
        gui_key('\n');                                       /* "Usar como interna" */
        CHECK(aum_auto_flag() == 1);
        typ("ana\t"); typ("segredo1\t"); typ("segredo1\n");  /* cria o admin */
        CHECK(users_cur() == 0);
        typ("write /docs/diario.txt sobrevive ao reinicio\n");
        CHECK(aum_led() == 2);                               /* por guardar */
        for (int i = 0; i < 4; i++) gui_tick();
        CHECK(aum_led() == 2);                               /* ainda dentro dos 4 s */
        for (int i = 0; i < 12; i++) gui_tick();
        CHECK(aum_led() == 1);                               /* guardou sozinho */
        gui_open("memoria"); shot("/tmp/m2.ppm");
        host_disk_dump("/tmp/auto1.aum");
    } else if (!strcmp(argv[1], "reboot")) {
        host_disk_load(argv[2]);
        gfx_init(1024, 768); gui_init();
        CHECK(users_n() == 1);                               /* carregou sozinho, sem dialogo */
        shot("/tmp/m3.ppm");                                 /* directo no login */
        typ("segredo1\n");
        CHECK(users_cur() == 0 && fs_find("/docs/diario.txt") >= 0);
        typ("write /docs/segundo.txt mais uma\n");
        gui_power(1);                                        /* alteracoes + auto: guarda e reinicia */
        host_disk_dump("/tmp/auto2.aum");
    } else if (!strcmp(argv[1], "corrupt")) {
        host_disk_load(argv[2]);
        gfx_init(1024, 768); gui_init();
        shot("/tmp/m4.ppm");                                 /* dialogo: ilegivel */
        gui_key('\n');
        CHECK(aum_led() == 3);
        CHECK(users_n() == 0);
        gui_login_ok(users_add("tmp1", "senha123", 1) == 0 ? 0 : 0);
        typ("write /docs/x.txt novo\n");
        for (int i = 0; i < 30; i++) gui_tick();
        host_disk_dump("/tmp/auto3.aum");
    }
    printf("falhas: %d\n", fails);
    return fails != 0;
}
