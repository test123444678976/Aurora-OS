/* Sistema de ficheiros em RAM com pastas. Os dados vivem num "pool" compacto. */
#include "os.h"
#define N_MAX 64
#define POOL_SIZE (2 * 1024 * 1024)
typedef struct { int used, isdir, parent, off, len; char name[FS_NAME]; } Node;
static Node N[N_MAX];
static uint8_t *pool;
static int pool_used, version, ready;

/* ---- permissoes: um utilizador normal so ve /home/<o seu nome>; o admin e o modo "raw" veem tudo ---- */
static int sess, raw, view_admin;
static char view_name[16];
static int homeid(void) {
    for (int i = 1; i < N_MAX; i++) if (N[i].used && N[i].parent == 0 && N[i].isdir && !kstrcmp(N[i].name, "home")) return i;
    return -1;
}
static int full_view(void) { return raw || !sess || view_admin; }
static int hidden(int id) {
    if (full_view() || id <= 0) return 0;
    int h = homeid();
    if (h < 0) return 0;
    for (int c = id; c > 0; c = N[c].parent) {
        if (N[c].parent == h) return kstrcmp(N[c].name, view_name) != 0;
        if (N[c].parent < 0) break;
    }
    return 0;
}
static int is_home_node(int id) { int h = homeid(); return h >= 0 && id > 0 && (id == h || N[id].parent == h); }
int fs_raw(int on) { int o = raw; raw = on; return o; }
void fs_set_user(const char *name, int admin) { sess = 1; view_admin = admin; kstrcpy(view_name, name); }
void fs_set_nouser(void) { sess = 0; view_admin = 0; view_name[0] = 0; }
void fs_touch(void) { version++; }
int fs_valid(int id) { return id >= 0 && id < N_MAX && N[id].used && !hidden(id); }
int fs_isdir(int id) { return fs_valid(id) && N[id].isdir; }
int fs_size(int id) { return fs_valid(id) && !N[id].isdir ? N[id].len : 0; }
const char *fs_nm(int id) { return fs_valid(id) ? N[id].name : ""; }
int fs_par(int id) { return fs_valid(id) ? N[id].parent : -1; }
int fs_used(void) { return pool_used; }
int fs_cap(void) { return POOL_SIZE; }
int fs_ver(void) { return version; }

static int okname(const char *n) {
    int l = kstrlen(n);
    if (l == 0 || l >= FS_NAME) return 0;
    if (!kstrcmp(n, ".") || !kstrcmp(n, "..")) return 0;
    for (int i = 0; i < l; i++) if ((unsigned char)n[i] < 32 || n[i] == '/') return 0;
    return 1;
}
int fs_child(int dir, const char *name) {
    for (int i = 1; i < N_MAX; i++)
        if (N[i].used && N[i].parent == dir && !kstrcmp(N[i].name, name)) return hidden(i) ? -1 : i;
    return -1;
}
int fs_mk(int dir, const char *name, int isdir) {
    if (!fs_isdir(dir) || !okname(name) || fs_child(dir, name) >= 0) return -1;
    if (dir == homeid() && !full_view()) return -1;      /* so o admin cria pastas pessoais */
    int i;
    for (i = 1; i < N_MAX && N[i].used; i++) ;
    if (i == N_MAX) return -1;
    N[i].used = 1; N[i].isdir = isdir; N[i].parent = dir; N[i].off = 0; N[i].len = 0;
    kstrcpy(N[i].name, name);
    version++;
    return i;
}
static void data_free(int i) {
    int off = N[i].off, len = N[i].len;
    if (!len) return;
    for (int k = off; k < pool_used - len; k++) pool[k] = pool[k + len];
    for (int j = 1; j < N_MAX; j++)
        if (N[j].used && !N[j].isdir && N[j].len && N[j].off > off) N[j].off -= len;
    pool_used -= len;
    N[i].len = 0;
}
int fs_wr(int id, const void *d, int len) {
    if (!fs_valid(id) || N[id].isdir || len < 0) return -1;
    if (pool_used - N[id].len + len > POOL_SIZE) return -1;
    data_free(id);
    const uint8_t *s = (const uint8_t *)d;
    for (int k = 0; k < len; k++) pool[pool_used + k] = s[k];
    N[id].off = pool_used; N[id].len = len; pool_used += len;
    version++;
    return 0;
}
int fs_rd(int id, void *buf, int max) {
    if (!fs_valid(id) || N[id].isdir) return -1;
    int n = N[id].len < max ? N[id].len : max;
    uint8_t *d = (uint8_t *)buf;
    for (int k = 0; k < n; k++) d[k] = pool[N[id].off + k];
    return n;
}
int fs_rd_at(int id, int off, void *buf, int n) {
    if (!fs_valid(id) || N[id].isdir || off < 0 || off >= N[id].len) return 0;
    if (n > N[id].len - off) n = N[id].len - off;
    uint8_t *d = (uint8_t *)buf;
    for (int k = 0; k < n; k++) d[k] = pool[N[id].off + off + k];
    return n;
}
int fs_append(int id, const void *d, int n) {            /* so para ficheiros construidos em sequencia */
    if (!fs_valid(id) || N[id].isdir || n < 0 || pool_used + n > POOL_SIZE) return -1;
    if (N[id].len == 0) N[id].off = pool_used;
    else if (N[id].off + N[id].len != pool_used) return -1;
    const uint8_t *s = (const uint8_t *)d;
    for (int k = 0; k < n; k++) pool[pool_used + k] = s[k];
    N[id].len += n; pool_used += n; version++;
    return 0;
}
int fs_count(void) { int c = 0; for (int i = 1; i < N_MAX; i++) if (N[i].used) c++; return c; }
void fs_reset(void) {
    for (int i = 1; i < N_MAX; i++) N[i].used = 0;
    pool_used = 0; version++;
}
int fs_rm(int id) {
    if (id <= 0 || !fs_valid(id)) return -1;
    if (is_home_node(id) && !full_view()) return -1;
    if (N[id].isdir) for (int i = 1; i < N_MAX; i++) if (N[i].used && N[i].parent == id) return -2;
    data_free(id);
    N[id].used = 0;
    version++;
    return 0;
}
int fs_ren(int id, const char *name) {
    if (id <= 0 || !fs_valid(id) || !okname(name) || (is_home_node(id) && !raw)) return -1;
    int c = fs_child(N[id].parent, name);
    if (c >= 0 && c != id) return -1;
    kstrcpy(N[id].name, name);
    version++;
    return 0;
}
int fs_move(int id, int newdir) {
    if (id <= 0 || !fs_valid(id) || !fs_isdir(newdir) || (is_home_node(id) && !raw)) return -1;
    if (newdir == homeid() && !full_view()) return -1;
    for (int p = newdir; p >= 0; p = N[p].parent) if (p == id) return -1;   /* nao entrar em si proprio */
    int c = fs_child(newdir, N[id].name);
    if (c >= 0 && c != id) return -1;
    N[id].parent = newdir;
    version++;
    return 0;
}
int fs_list(int dir, int *ids, int max) {
    int n = 0;
    for (int i = 1; i < N_MAX && n < max; i++) if (N[i].used && N[i].parent == dir && !hidden(i)) ids[n++] = i;
    for (int a = 1; a < n; a++) {                       /* insercao: pastas primeiro, depois por nome */
        int v = ids[a], b = a - 1;
        while (b >= 0) {
            int x = ids[b];
            int after = (N[x].isdir == N[v].isdir) ? kstrcmp(N[x].name, N[v].name) > 0 : (!N[x].isdir && N[v].isdir);
            if (!after) break;
            ids[b + 1] = x; b--;
        }
        ids[b + 1] = v;
    }
    return n;
}
void fs_path(int id, char *out) {
    int chain[16], n = 0;
    while (fs_valid(id) && id != 0 && n < 16) { chain[n++] = id; id = N[id].parent; }
    int o = 0;
    if (n == 0) { out[0] = '/'; out[1] = 0; return; }
    while (n--) { out[o++] = '/'; for (const char *s = N[chain[n]].name; *s; s++) out[o++] = *s; }
    out[o] = 0;
}
void fs_abs(int cwd, const char *p, char *out) {
    char t[160]; int l = 0;
    t[0] = 0;
    if (p[0] != '/') { fs_path(cwd, t); l = kstrlen(t); if (l == 1) { t[0] = 0; l = 0; } }
    while (*p) {
        while (*p == '/') p++;
        char c[FS_NAME + 4]; int cl = 0;
        while (*p && *p != '/') { if (cl < FS_NAME + 2) c[cl++] = *p; p++; }
        c[cl] = 0;
        if (!cl || !kstrcmp(c, ".")) continue;
        if (!kstrcmp(c, "..")) { while (l > 0 && t[l - 1] != '/') l--; if (l > 0) l--; t[l] = 0; continue; }
        if (l + 1 + cl < 150) { t[l++] = '/'; for (int k = 0; k < cl; k++) t[l++] = c[k]; t[l] = 0; }
    }
    if (l == 0) { out[0] = '/'; out[1] = 0; } else kstrcpy(out, t);
}
int fs_find(const char *abspath) {
    char t[160]; fs_abs(0, abspath, t);
    int cur = 0;
    const char *p = t;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        char c[FS_NAME + 4]; int cl = 0;
        while (*p && *p != '/') { if (cl < FS_NAME + 2) c[cl++] = *p; p++; }
        c[cl] = 0;
        cur = fs_child(cur, c);
        if (cur < 0) return -1;
    }
    return cur;
}
int fs_parent_of(const char *abspath, char *name) {
    char t[160]; fs_abs(0, abspath, t);
    int l = kstrlen(t);
    while (l > 0 && t[l - 1] != '/') l--;
    if (kstrlen(t) <= 1) return -1;
    kstrcpy(name, t + l);
    t[l > 1 ? l - 1 : 1] = 0;
    return fs_find(t);
}
int fs_mkpath(const char *abspath, int isdir) {
    char name[FS_NAME + 4];
    int par = fs_parent_of(abspath, name);
    return par < 0 ? -1 : fs_mk(par, name, isdir);
}
int fs_save(const char *abspath, const void *d, int len) {
    int id = fs_find(abspath);
    if (id >= 0 && fs_isdir(id)) return -1;
    if (id < 0) { id = fs_mkpath(abspath, 0); if (id < 0) return -1; }
    if (fs_wr(id, d, len) < 0) return -1;
    return 0;
}
void fs_defaults(void) {
    fs_mk(0, "home", 1);
    fs_mk(0, "docs", 1);
    fs_mk(0, "imagens", 1);
    const char *a = "Bem-vindo ao AuroraOS!\n\nAtalhos do Bloco de Notas:\n  Ctrl+S  guardar (em RAM)\n  Ctrl+O  abrir\n\nExplorador: n ficheiro, p pasta, r renomear, m mover/colar,\n  Enter abrir, Backspace subir, Del apagar.\nPaint: guarda em /imagens (.aimg); abre-as pelo Explorador.\n\nMemória (.aum): a app Memória guarda tudo num disco .aum\nligado à máquina e carrega-o no arranque.\n";
    fs_save("/leia-me.txt", a, kstrlen(a));
    const char *b = "Olá, mundo!\n";
    fs_save("/docs/ola.txt", b, kstrlen(b));
}
void fs_init(void) {
    if (ready) return;
    ready = 1;
    pool = (uint8_t *)hal_alloc(POOL_SIZE);
    N[0].used = 1; N[0].isdir = 1; N[0].parent = -1; N[0].name[0] = 0;
    fs_defaults();
}
