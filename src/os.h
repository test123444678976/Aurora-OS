#ifndef OS_H
#define OS_H
#include <stdint.h>
#include <stddef.h>

/* ---------- teclas especiais (alem de Latin-1) ---------- */
enum { K_UP = 0x100, K_DOWN, K_LEFT, K_RIGHT, K_DEL, K_HOME, K_END };

/* ---------- HAL: implementado por kernel.c (x86) ou pelo teste no host ---------- */
void       *hal_alloc(uint32_t size);                 /* memoria zerada */
void        hal_present(int x, int y, int w, int h);  /* copia back -> ecra */
void        hal_time(int *h, int *m, int *s);
void        hal_date(int *d, int *mo, int *y);
void        hal_reboot(void);
void        hal_shutdown(void);
const char *hal_cpu(void);
const char *hal_video(void);
uint32_t    hal_ms(void);                             /* milissegundos desde o arranque */

/* ---------- gfx ---------- */
extern uint32_t *back;
extern int SW, SH;
#define RGB(r, g, b) (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
void gfx_init(int w, int h);
void gx_px(int x, int y, uint32_t c);
void gx_fill(int x, int y, int w, int h, uint32_t c);
void gx_rect(int x, int y, int w, int h, uint32_t c);
void gx_shade(int x, int y, int w, int h, uint32_t c, int a); /* a: 0..256 */
void gx_vgrad(int x, int y, int w, int h, uint32_t c1, uint32_t c2);
void gx_rrect(int x, int y, int w, int h, int r, uint32_t c);
void gx_circle(int cx, int cy, int r, uint32_t c);
void gx_line(int x0, int y0, int x1, int y1, uint32_t c);
void gx_char(int x, int y, int ch, uint32_t c, int scale);
int  gx_text(int x, int y, const char *s, uint32_t c, int scale);
void gx_textb(int x, int y, const char *s, uint32_t c);          /* "negrito" */
int  gx_textw(const char *s, int scale);
uint32_t gx_mix(uint32_t d, uint32_t s, int a);

/* ---------- gui ---------- */
void gui_init(void);
void gui_key(int k);
void gui_mouse(int dx, int dy, int btn);
void gui_tick(void);
void gui_flush(void);
void gui_open(const char *name);

/* ---------- sistema de ficheiros em RAM, com pastas (fs.c) ---------- */
#define FS_MAX 8192          /* limite do texto editavel (bloco de notas) */
#define FS_NAME 20
void        fs_init(void);
int         fs_valid(int id);
int         fs_isdir(int id);
int         fs_size(int id);
const char *fs_nm(int id);
int         fs_par(int id);                                  /* pasta-mae (-1 na raiz) */
int         fs_child(int dir, const char *name);
int         fs_find(const char *abspath);                    /* id ou -1 */
int         fs_mk(int dir, const char *name, int isdir);     /* id ou -1 */
int         fs_mkpath(const char *abspath, int isdir);
int         fs_parent_of(const char *abspath, char *name);   /* id da pasta-mae; escreve o nome */
int         fs_wr(int id, const void *d, int len);           /* 0 ok */
int         fs_rd(int id, void *buf, int max);               /* bytes lidos */
int         fs_save(const char *abspath, const void *d, int len);
int         fs_rm(int id);                                   /* 0 ok, -1 nao existe, -2 pasta nao vazia */
int         fs_ren(int id, const char *name);
int         fs_move(int id, int newdir);
int         fs_list(int dir, int *ids, int max);             /* pastas primeiro, ordem alfabetica */
void        fs_path(int id, char *out);                      /* caminho absoluto */
void        fs_abs(int cwd, const char *p, char *out);       /* normaliza ., .., relativo */
int         fs_used(void);                                   /* bytes de dados em uso */
int         fs_cap(void);
int         fs_ver(void);
void        fs_touch(void);                                  /* marca alteracoes (ex.: contas) */
int         fs_raw(int on);                                  /* 1 = ignora permissoes (devolve o estado anterior) */
void        fs_set_user(const char *name, int admin);        /* inicia sessao no sistema de ficheiros */
void        fs_set_nouser(void);                             /* sem sessao: acesso total */
int         fs_rd_at(int id, int off, void *buf, int n);
int         fs_append(int id, const void *d, int n);
int         fs_count(void);
void        fs_reset(void);                                  /* apaga tudo (fica so a raiz) */
void        fs_defaults(void);                               /* recria pastas e ficheiros de exemplo */

/* ---------- discos ATA (hal) e memoria .aum (aum.c) ---------- */
int         hal_disk_count(void);
uint32_t    hal_disk_sectors(int d);
int         hal_disk_read(int d, uint32_t lba, void *buf, int n);          /* 0 ok */
int         hal_disk_write(int d, uint32_t lba, const void *buf, int n);
void        hal_disk_flush(int d);
enum { AUM_NODISK, AUM_BLANK, AUM_VALID, AUM_BAD };
typedef struct { int state, disk, foreign, count, users, ver, autom, y, mo, d, h, mi; uint32_t sectors, payload; } AumInfo;
void        aum_probe(AumInfo *i);
int         aum_save(int *need_kb);      /* 0 ok, -1 sem disco, -2 pequeno, -3 erro de E/S */
int         aum_load(void);              /* 0 ok, -1 sem .aum valido, -2 corrompido */
int         aum_set_auto(int on);        /* memoria interna automatica: 0 ok, -1 sem disco, -3 erro */
int         aum_auto_flag(void);         /* ligada no disco */
int         aum_auto_active(void);       /* ligada e nao suspensa por erro */
int         aum_synced(void);            /* a RAM corresponde ao que esta no disco (foi carregada/guardada) */
int         aum_led(void);               /* 0 sem disco, 1 guardado, 2 por guardar, 3 erro/suspenso */
void        aum_suspend(void);           /* para o auto-guardar (ex.: memoria ilegivel) */
void        aum_autosave_tick(uint32_t ms);
int         aum_dirty(void);             /* ha alteracoes por guardar (e existe disco) */
void        aum_mark_clean(void);
uint32_t    hal_random(void);
/* contas (users.c) */
uint32_t    users_color(int i);
int         users_n(void);
const char *users_name(int i);
int         users_admin(int i);
int         users_cur(void);
void        users_set_cur(int i);
void        users_home(int i, char *out);
int         users_find(const char *name);
int         users_valid_name(const char *n);
int         users_add(const char *name, const char *pw, int admin);   /* 0 ok, -1 nome, -2 existe, -3 cheio, -4 pw curta */
int         users_check(int i, const char *pw);
int         users_setpw(int i, const char *pw);
int         users_setadmin(int i, int a);                              /* -2: ultimo administrador */
int         users_del(int i);                                          /* -2: ultimo administrador */
void        users_ensure_homes(void);
void        users_emit(void (*put)(void *, const void *, int), void *ctx);
int         users_parse(int (*get)(void *, void *, int), void *ctx);
void        sha256(const uint8_t *d, int len, uint8_t out[32]);
/* ecra de login (login.c) */
void        login_reset(void);
int         login_hover(int mx, int my);
void        login_click(int mx, int my);
void        login_key(int k);
void        login_draw(int hv);
/* sessao (gui.c) */
void        gui_login_ok(int uid);
void        gui_logout(void);
void        gui_session_check(void);
void        gui_power(int reboot);       /* pede para guardar se houver alteracoes */
void        gui_request_load(void);
void        gui_request_auto(int on);
int         mem_do_save(void);
void        mem_do_auto(int on);
void        mem_set_msg(const char *m, int ok);
void        apps_fs_changed(void);                                    /* muda a cada escrita */

/* ---------- pequenas rotinas de string ---------- */
static inline int kstrlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static inline int kstrcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}
static inline void kstrcpy(char *d, const char *s) { while ((*d++ = *s++)) ; }
static inline int kitoa(int v, char *buf) {
    char t[12]; int n = 0, neg = 0, i = 0;
    unsigned u;
    if (v < 0) { neg = 1; u = (unsigned)(-(v + 1)) + 1u; } else u = (unsigned)v;
    do { t[n++] = '0' + (u % 10); u /= 10; } while (u);
    if (neg) buf[i++] = '-';
    while (n) buf[i++] = t[--n];
    buf[i] = 0;
    return i;
}
static inline void kpad2(char *d, int v) { d[0] = '0' + (v / 10) % 10; d[1] = '0' + v % 10; }
#endif
