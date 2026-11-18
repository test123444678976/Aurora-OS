#include "../src/os.h"
#include <stdio.h>
#include <stdlib.h>
void *hal_alloc(uint32_t n) { return calloc(1, n); }
void hal_present(int x, int y, int w, int h) { if (x < 0 || y < 0 || x + w > SW || y + h > SH) { puts("PRESENT FORA"); abort(); } }
void hal_time(int *h, int *m, int *s) { *h = 23; *m = 59; *s = rand() % 60; }
void hal_date(int *d, int *mo, int *y) { *d = 1; *mo = 1; *y = 2027; }
uint32_t hal_ms(void) { static uint32_t t; return t += 37; }
void host_disk_blank(int); void host_disk_none(void);
void hal_reboot(void) {} void hal_shutdown(void) {}
const char *hal_cpu(void) { return "cpu"; } const char *hal_video(void) { return "v"; }
static void dump(const char *n) { gui_flush(); FILE *f = fopen(n, "wb"); fprintf(f, "P6\n%d %d\n255\n", SW, SH);
  for (int i = 0; i < SW * SH; i++) { unsigned c = back[i]; fputc(c >> 16, f); fputc(c >> 8, f); fputc(c, f); } fclose(f); }
int main(int argc, char **argv) {
    int W = argc > 1 ? atoi(argv[1]) : 1024, H = argc > 2 ? atoi(argv[2]) : 768;
    host_disk_blank(4); gfx_init(W, H); gui_init(); gui_key('\n'); users_add("admin1", "senha123", 1); users_add("user2", "senha456", 0); gui_login_ok(0); gui_flush();
    srand(1);
    for (int i = 0; i < 400; i++) { char b[40]; for (int k = 0; k < 8; k++) gui_key("echo linha "[k]); for (char *p = b + sprintf(b, "%d\n", i), *q = b; q < p; q++) gui_key(*q); }
    for (int i = 0; i < 300; i++) gui_key('x');
    gui_key('\n'); gui_key(K_UP); gui_key('\b'); gui_key('\n');
    for (int i = 0; i < 1200; i++) {
        int r = rand() % 10;
        if (r < 6) gui_mouse(rand() % 120 - 60, rand() % 120 - 60, rand() % 2);
        else if (r < 9) gui_key(rand() % 512 < 300 ? 32 + rand() % 200 : K_UP + rand() % 7);
        else { gui_key('\n'); gui_tick(); }
        gui_flush();
    }
    /* longas escritas no bloco de notas */
    gui_open("notas");
    for (int i = 0; i < 9000; i++) gui_key(i % 71 == 0 ? '\n' : 'a' + i % 26);
    for (int i = 0; i < 200; i++) { gui_key(K_UP); gui_key(K_LEFT); } for (int i = 0; i < 200; i++) { gui_key(K_DOWN); gui_key(K_END); gui_key(K_HOME); gui_key('\b'); }
    gui_open("calc"); gui_key('9'); gui_key('/'); gui_key('0'); gui_key('='); gui_key('c');
    for (int i = 0; i < 40; i++) gui_key("123456789*+-/="[i % 14]);
    gui_open("paint"); for (int i = 0; i < 40; i++) gui_key("+-c123"[i % 6]);
    /* snake + ficheiros + maximizar */
    gui_open("snake"); gui_open("ficheiros"); for (int i = 0; i < 400; i++) { gui_key("nr\n\bxyz.t"[rand()%10]); gui_key(rand()%3? K_DOWN: K_UP); gui_mouse(rand()%200-100, rand()%200-100, rand()%2); gui_flush(); }
    for (int i = 0; i < 3000; i++) { gui_key(K_UP + rand() % 4); gui_tick(); gui_flush(); }
    const char *cmds[] = { "write a.txt ola", "ls", "cat a.txt", "edit a.txt", "rm a.txt", "cat a.txt", "uptime", "write b hello world", "ls", "mkdir x", "mkdir x/y", "cd x/y", "write z.txt zzz", "tree", "cd ../..", "mv x/y/z.txt /docs", "cp /docs/z.txt /docs/z2.txt", "rm x", "rm x/y", "rm x", "cp /imagens/exemplo.aimg /imagens/c.aimg", "edit /imagens/c.aimg", "auto on", "auto off", "auto", "cd /nada", "logout", "whoami", "users", "mkdir /nada/a/b", "mv /docs /docs/ola.txt", "mv / /docs", "save", "mkdir q", "load", "aum", "df", "ls /imagens", "cat /imagens/c.aimg", "tree" };
    for (int i = 0; i < 34; i++) { gui_open("terminal"); for (const char *p = cmds[i]; *p; p++) gui_key(*p); gui_key('\n'); }
    gui_open("notas"); gui_key('x'); gui_key(19); gui_key(15);
    for (int r = 0; r < 20; r++) { gui_mouse(500 - 0, 100, 0); }
    dump("/tmp/st.ppm");
    puts("stress ok");
    return 0;
}
