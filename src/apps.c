#include "apps.h"

static int wcw[NAPPS], wch[NAPPS];

/* ====================================================================
 *  TERMINAL
 * ==================================================================== */
#define TL 150
#define TC 110
static char tl[TL][TC + 1];
static int tn = 1, tcols = 80, trows = 24, tinlen = 0, tcwd = 0;
static char tin[100], tlast[100], tprompt[64] = "?@aurora:/> ";

static char *tcur(void) { return tl[tn - 1]; }
static void t_nl(void) {
    if (tn < TL) tn++;
    else {
        for (int i = 0; i < TL - 1; i++) kstrcpy(tl[i], tl[i + 1]);
    }
    tl[tn - 1][0] = 0;
}
static void t_putc(int c) {
    if (c == '\n') { t_nl(); return; }
    int n = kstrlen(tcur());
    if (n >= tcols) { t_nl(); n = 0; }
    tcur()[n] = (char)c;
    tcur()[n + 1] = 0;
}
static void t_puts(const char *s) { while (*s) t_putc((unsigned char)*s++); }
static void t_setprompt(void) {
    char p[160];
    if (!fs_valid(tcwd)) tcwd = 0;
    fs_path(tcwd, p);
    int l = kstrlen(p);
    kstrcpy(tprompt, users_cur() >= 0 ? users_name(users_cur()) : "aurora");
    kstrcpy(tprompt + kstrlen(tprompt), "@aurora:");
    int o = kstrlen(tprompt);
    if (l > 26) { kstrcpy(tprompt + o, "..."); kstrcpy(tprompt + o + 3, p + l - 23); }
    else kstrcpy(tprompt + o, p);
    l = kstrlen(tprompt);
    tprompt[l] = '>'; tprompt[l + 1] = ' '; tprompt[l + 2] = 0;
}
static int has_ext(const char *n, const char *e) {
    int a = kstrlen(n), b = kstrlen(e);
    return a >= b && !kstrcmp(n + a - b, e);
}

static void t_tree(int dir, int depth) {
    int ids[64];
    int n = fs_list(dir, ids, 64);
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < depth * 2; k++) t_putc(' ');
        t_puts(fs_nm(ids[i]));
        if (fs_isdir(ids[i])) { t_putc('/'); t_putc('\n'); if (depth < 6) t_tree(ids[i], depth + 1); }
        else t_putc('\n');
    }
}

static void term_exec(char *cmd) {
    while (*cmd == ' ') cmd++;
    char *arg = cmd;
    while (*arg && *arg != ' ') arg++;
    if (*arg) { *arg++ = 0; while (*arg == ' ') arg++; }
    if (!*cmd) return;
    char ab[160], ab2[160];

    if (!kstrcmp(cmd, "help")) {
        t_puts("Comandos disponíveis:\n"
               "  help  clear  echo TXT  ver  fetch  cpu  time  date  uptime\n"
               "  pwd              mostra a pasta atual\n"
               "  cd PASTA         muda de pasta (.. sobe, / raiz)\n"
               "  ls [PASTA]       lista o conteúdo\n"
               "  tree             árvore de pastas\n"
               "  mkdir PASTA      cria uma pasta\n"
               "  cat F            mostra um ficheiro\n"
               "  write F TXT      escreve TXT em F\n"
               "  edit F           edita F (imagens abrem no Paint)\n"
               "  cp A B  mv A B   copiar / mover / renomear\n"
               "  rm F             apaga ficheiro ou pasta vazia\n"
               "  df               espaço usado\n"
               "  save / load      guarda / carrega a memória (.aum)\n"
               "  whoami  users    quem és / lista de utilizadores\n"
               "  logout           terminar sessão\n"
               "  aum              estado do disco .aum\n"
               "  auto on|off      memória interna automática\n"
               "  open APP         notas paint calc ficheiros memoria utilizadores snake sobre\n"
               "  reboot  shutdown\n");
    } else if (!kstrcmp(cmd, "clear")) {
        tn = 1; tl[0][0] = 0;
    } else if (!kstrcmp(cmd, "echo")) {
        t_puts(arg); t_putc('\n');
    } else if (!kstrcmp(cmd, "ver")) {
        t_puts("AuroraOS 1.0 - bootloader, kernel e interface escritos de raiz\n");
    } else if (!kstrcmp(cmd, "cpu")) {
        t_puts(hal_cpu()); t_putc('\n');
    } else if (!kstrcmp(cmd, "time")) {
        int h, m, s; char b[12];
        hal_time(&h, &m, &s);
        kpad2(b, h); b[2] = ':'; kpad2(b + 3, m); b[5] = ':'; kpad2(b + 6, s); b[8] = 0;
        t_puts(b); t_putc('\n');
    } else if (!kstrcmp(cmd, "date")) {
        int d, m, y; char b[12];
        hal_date(&d, &m, &y);
        kpad2(b, d); b[2] = '/'; kpad2(b + 3, m); b[5] = '/'; kitoa(y, b + 6);
        t_puts(b); t_putc('\n');
    } else if (!kstrcmp(cmd, "fetch")) {
        t_puts("    /\\         aurora@aurora-os\n"
               "   /  \\        ----------------\n"
               "  / /\\ \\       SO:     AuroraOS 1.0\n"
               " / ____ \\      Kernel: monolítico, 32-bit\n"
               "/_/    \\_\\     CPU:    ");
        t_puts(hal_cpu());
        t_puts("\n               Vídeo:  "); t_puts(hal_video());
        t_puts("\n               Teclado: PT (PS/2)   Rato: PS/2\n               Utilizador: ");
        t_puts(users_name(users_cur())); t_putc('\n');
    } else if (!kstrcmp(cmd, "uptime")) {
        uint32_t s = hal_ms() / 1000; char b[16];
        kitoa((int)(s / 3600), b); t_puts(b); t_puts("h ");
        kitoa((int)(s / 60 % 60), b); t_puts(b); t_puts("m ");
        kitoa((int)(s % 60), b); t_puts(b); t_puts("s\n");
    } else if (!kstrcmp(cmd, "pwd")) {
        fs_path(tcwd, ab); t_puts(ab); t_putc('\n');
    } else if (!kstrcmp(cmd, "cd")) {
        fs_abs(tcwd, *arg ? arg : "/", ab);
        int id = fs_find(ab);
        if (id < 0) t_puts("pasta não encontrada\n");
        else if (!fs_isdir(id)) t_puts("não é uma pasta\n");
        else tcwd = id;
    } else if (!kstrcmp(cmd, "ls")) {
        int dir = tcwd;
        if (*arg) { fs_abs(tcwd, arg, ab); dir = fs_find(ab); }
        if (dir < 0) t_puts("não encontrado\n");
        else if (!fs_isdir(dir)) { t_puts(fs_nm(dir)); t_putc('\n'); }
        else {
            int ids[64], n = fs_list(dir, ids, 64); char b[16];
            for (int i = 0; i < n; i++) {
                const char *nm = fs_nm(ids[i]);
                t_puts(nm);
                int l = kstrlen(nm);
                if (fs_isdir(ids[i])) { t_putc('/'); l++; }
                for (; l < 24; l++) t_putc(' ');
                if (fs_isdir(ids[i])) t_puts("<pasta>\n");
                else { kitoa(fs_size(ids[i]), b); t_puts(b); t_puts(" bytes\n"); }
            }
            if (!n) t_puts("(vazia)\n");
        }
    } else if (!kstrcmp(cmd, "tree")) {
        t_puts("/\n"); t_tree(0, 1);
    } else if (!kstrcmp(cmd, "mkdir")) {
        fs_abs(tcwd, arg, ab);
        if (!*arg || fs_mkpath(ab, 1) < 0) t_puts("erro: nome inválido, já existe ou pasta-mãe inexistente\n");
    } else if (!kstrcmp(cmd, "cat")) {
        static char buf[FS_MAX + 1];
        fs_abs(tcwd, arg, ab);
        int id = *arg ? fs_find(ab) : -1;
        if (id < 0) t_puts("ficheiro não encontrado\n");
        else if (fs_isdir(id)) t_puts("é uma pasta (usa 'ls')\n");
        else if (has_ext(fs_nm(id), ".aimg")) t_puts("é uma imagem: usa 'edit' para abrir no Paint\n");
        else {
            int n = fs_rd(id, buf, FS_MAX);
            for (int i = 0; i < n; i++) t_putc((unsigned char)buf[i]);
            if (n && buf[n - 1] != '\n') t_putc('\n');
        }
    } else if (!kstrcmp(cmd, "write")) {
        char *txt = arg;
        while (*txt && *txt != ' ') txt++;
        if (*txt) *txt++ = 0;
        static char buf[FS_MAX];
        int n = 0;
        while (txt[n] && n < FS_MAX - 1) { buf[n] = txt[n]; n++; }
        buf[n++] = '\n';
        fs_abs(tcwd, arg, ab);
        if (!*arg || fs_save(ab, buf, n) < 0) t_puts("erro: uso 'write ficheiro texto'\n");
    } else if (!kstrcmp(cmd, "edit")) {
        if (!*arg) { t_puts("uso: edit ficheiro\n"); }
        else {
            fs_abs(tcwd, arg, ab);
            int id = fs_find(ab);
            if (id < 0) id = fs_mkpath(ab, 0);
            if (id < 0 || fs_isdir(id)) t_puts("não foi possível abrir\n");
            else if (has_ext(ab, ".aimg")) { paint_open_file(ab); gui_open("paint"); }
            else { notes_open_file(ab); gui_open("notas"); }
        }
    } else if (!kstrcmp(cmd, "rm")) {
        fs_abs(tcwd, arg, ab);
        int id = *arg ? fs_find(ab) : -1;
        if (id == tcwd && id > 0) tcwd = fs_par(id);
        int rc = id < 0 ? -1 : fs_rm(id);
        if (rc == -1) t_puts("não encontrado\n");
        else if (rc == -2) t_puts("a pasta não está vazia\n");
    } else if (!kstrcmp(cmd, "cp") || !kstrcmp(cmd, "mv")) {
        int mv = cmd[0] == 'm';
        char *dst = arg;
        while (*dst && *dst != ' ') dst++;
        if (*dst) { *dst++ = 0; while (*dst == ' ') dst++; }
        fs_abs(tcwd, arg, ab);
        int src = *arg ? fs_find(ab) : -1;
        if (src < 0 || !*dst) { t_puts("uso: cp|mv origem destino\n"); }
        else {
            fs_abs(tcwd, dst, ab2);
            int did = fs_find(ab2);
            char name[FS_NAME + 4];
            if (did >= 0 && fs_isdir(did)) kstrcpy(name, fs_nm(src));
            int ok = -1, pdir = did;
            if (!(did >= 0 && fs_isdir(did))) {
                if (did >= 0) { t_puts("o destino já existe\n"); return; }
                pdir = fs_parent_of(ab2, name);
            }
            if (pdir < 0) t_puts("pasta de destino inexistente\n");
            else if (mv) {
                if (src == tcwd) tcwd = fs_par(src);
                ok = fs_move(src, pdir);
                if (ok == 0 && kstrcmp(fs_nm(src), name)) ok = fs_ren(src, name);
                if (ok < 0) t_puts("não foi possível mover\n");
            } else if (fs_isdir(src)) t_puts("cp não copia pastas\n");
            else {
                int sz = fs_size(src);
                char *tmp = (char *)app_scratch(sz);
                int nid = tmp ? fs_mk(pdir, name, 0) : -1;
                if (nid < 0) t_puts("não foi possível copiar\n");
                else { fs_rd(src, tmp, sz); if (fs_wr(nid, tmp, sz) < 0) { fs_rm(nid); t_puts("sem espaço\n"); } }
            }
        }
    } else if (!kstrcmp(cmd, "df")) {
        char b[16];
        t_puts("dados:   "); kitoa(fs_used() / 1024, b); t_puts(b); t_puts(" KB de "); kitoa(fs_cap() / 1024, b); t_puts(b); t_puts(" KB\n");
    } else if (!kstrcmp(cmd, "whoami")) {
        t_puts(users_name(users_cur())); t_puts(users_admin(users_cur()) ? " (administrador)\n" : " (utilizador)\n");
    } else if (!kstrcmp(cmd, "users")) {
        for (int i = 0; i < users_n(); i++) {
            t_puts(i == users_cur() ? "* " : "  "); t_puts(users_name(i));
            t_puts(users_admin(i) ? "  administrador\n" : "  utilizador\n");
        }
    } else if (!kstrcmp(cmd, "logout")) {
        gui_logout();
    } else if (!kstrcmp(cmd, "save")) {
        mem_do_save(); t_puts(mem_msg()); t_putc('\n');
    } else if (!kstrcmp(cmd, "load")) {
        if (aum_dirty()) { gui_request_load(); t_puts("(confirma no diálogo)\n"); }
        else { mem_do_load(); t_puts(mem_msg()); t_putc('\n'); }
    } else if (!kstrcmp(cmd, "auto")) {
        if (!kstrcmp(arg, "on") || !kstrcmp(arg, "off")) { gui_request_auto(arg[1] == 'n'); t_puts(mem_msg()); t_putc('\n'); }
        else t_puts(aum_auto_flag() ? "memória interna: ligada\n" : "memória interna: desligada\n");
    } else if (!kstrcmp(cmd, "aum")) {
        AumInfo in; aum_probe(&in);
        static const char *st[4] = { "nenhum disco .aum ligado", "disco .aum novo (vazio)", "memória guardada encontrada", "cabeçalho inválido" };
        char b[16];
        t_puts(st[in.state]); t_putc('\n');
        if (in.state == AUM_VALID) { kitoa(in.count, b); t_puts(b); t_puts(" itens, "); kitoa((int)((in.payload + 1023) / 1024), b); t_puts(b); t_puts(" KB\n"); }
        t_puts(aum_auto_flag() ? "memória interna: ligada (guarda sozinha)\n" : "memória interna: desligada\n");
        t_puts(aum_dirty() ? "há alterações por guardar\n" : "sem alterações por guardar\n");
    } else if (!kstrcmp(cmd, "open")) {
        if (*arg) gui_open(arg); else t_puts("uso: open notas|paint|calc|ficheiros|memoria|utilizadores|snake|sobre\n");
    } else if (!kstrcmp(cmd, "reboot")) {
        if (!aum_dirty()) t_puts("A reiniciar...\n"); gui_power(1);
    } else if (!kstrcmp(cmd, "shutdown")) {
        if (!aum_dirty()) t_puts("A desligar...\n"); gui_power(0);
    } else {
        t_puts("comando desconhecido: "); t_puts(cmd); t_puts("  (escreve 'help')\n");
    }
}

static void term_key(int k) {
    int maxin = tcols - kstrlen(tprompt) - 3;
    if (maxin > 90) maxin = 90;
    if (k == '\n') {
        t_putc('\n');
        tin[tinlen] = 0;
        if (tinlen) kstrcpy(tlast, tin);
        term_exec(tin);
        tinlen = 0;
        t_setprompt();
        t_puts(tprompt);
    } else if (k == '\b') {
        if (tinlen > 0) { tinlen--; tcur()[kstrlen(tcur()) - 1] = 0; }
    } else if (k == K_UP) {
        while (tinlen > 0) { tinlen--; tcur()[kstrlen(tcur()) - 1] = 0; }
        for (const char *p = tlast; *p && tinlen < maxin; p++) { tin[tinlen++] = *p; t_putc((unsigned char)*p); }
    } else if (k >= 32 && k < 256 && k != 127 && tinlen < maxin) {
        tin[tinlen++] = (char)k;
        t_putc(k);
    }
}

static void term_draw(int x, int y, int w, int h) {
    gx_fill(x, y, w, h, RGB(14, 17, 24));
    int vis = tn < trows ? tn : trows;
    int first = tn - vis;
    for (int i = 0; i < vis; i++) {
        const char *s = tl[first + i];
        int ty = y + 6 + i * 16, tx = x + 8;
        int pl = 0;
        for (int j = 1; j < 14 && s[j]; j++)
            if (s[j] == '@' && s[j+1] == 'a' && s[j+2] == 'u' && s[j+3] == 'r' && s[j+4] == 'o' && s[j+5] == 'r' && s[j+6] == 'a' && s[j+7] == ':') {
                for (int q = j + 8; s[q]; q++) if (s[q] == '>' && s[q + 1] == ' ') { pl = q + 2; break; }
                break;
            }
        if (pl) {
            for (int j = 0; j < pl; j++) gx_char(tx + j * 8, ty, (unsigned char)s[j], RGB(94, 230, 196), 1);
            gx_text(tx + pl * 8, ty, s + pl, RGB(235, 238, 245), 1);
        } else gx_text(tx, ty, s, RGB(190, 214, 190), 1);
    }
    int cx = x + 8 + kstrlen(tcur()) * 8, cy = y + 6 + (vis - 1) * 16;
    gx_fill(cx, cy + 1, 8, 14, RGB(94, 230, 196));
}

/* ====================================================================
 *  BLOCO DE NOTAS
 * ==================================================================== */
#define NMAX 8192
static char nt[NMAX];
static int nlen, ncur, ncols = 60, nrows = 20, ntop;
static char nfile[60] = "/nota.txt", nmsg[24];

static void n_pos(int idx, int *row, int *col) {
    int r = 0, c = 0;
    for (int i = 0; i < idx; i++) {
        if (c >= ncols) { r++; c = 0; }
        if (nt[i] == '\n') { r++; c = 0; } else c++;
    }
    if (c >= ncols) { r++; c = 0; }
    *row = r; *col = c;
}
static int n_idx(int trow, int tcol) {
    int r = 0, c = 0;
    for (int i = 0; i <= nlen; i++) {
        int rr = r, cc = c;
        if (cc >= ncols) { rr++; cc = 0; }
        if (rr > trow) return i > 0 ? i - 1 : 0;
        if (rr == trow && cc >= tcol) return i;
        if (i == nlen) break;
        r = rr; c = cc;
        if (nt[i] == '\n') { r++; c = 0; } else c++;
    }
    return nlen;
}
void notes_open_file(const char *path) {
    int i = 0;
    while (path[i] && i < 59) { nfile[i] = path[i]; i++; }
    nfile[i] = 0;
    int id = fs_find(nfile);
    int n = (id >= 0 && !fs_isdir(id)) ? fs_rd(id, nt, NMAX - 2) : -1;
    nlen = n < 0 ? 0 : n; ncur = 0; ntop = 0;
    kstrcpy(nmsg, n < 0 ? "novo ficheiro" : "aberto");
}
static void notes_key(int k) {
    int r, c;
    if (k == 19) { kstrcpy(nmsg, fs_save(nfile, nt, nlen) == 0 ? "guardado" : "erro ao guardar"); return; }
    if (k == 15) { notes_open_file(nfile); return; }
    if (k == '\b') {
        if (ncur > 0) { for (int i = ncur - 1; i < nlen - 1; i++) nt[i] = nt[i + 1]; nlen--; ncur--; }
    } else if (k == K_DEL) {
        if (ncur < nlen) { for (int i = ncur; i < nlen - 1; i++) nt[i] = nt[i + 1]; nlen--; }
    } else if (k == K_LEFT) { if (ncur > 0) ncur--; }
    else if (k == K_RIGHT) { if (ncur < nlen) ncur++; }
    else if (k == K_UP)   { n_pos(ncur, &r, &c); if (r > 0) ncur = n_idx(r - 1, c); }
    else if (k == K_DOWN) { n_pos(ncur, &r, &c); ncur = n_idx(r + 1, c); }
    else if (k == K_HOME) { n_pos(ncur, &r, &c); ncur = n_idx(r, 0); }
    else if (k == K_END)  { n_pos(ncur, &r, &c); ncur = n_idx(r, ncols); }
    else if ((k == '\n' || k == 9 || (k >= 32 && k < 256 && k != 127)) && nlen < NMAX - 2) {
        int ch = k == 9 ? ' ' : k;
        for (int i = nlen; i > ncur; i--) nt[i] = nt[i - 1];
        nt[ncur++] = (char)ch; nlen++;
    }
}
static void notes_draw(int x, int y, int w, int h) {
    gx_fill(x, y, w, h, RGB(252, 252, 250));
    gx_fill(x, y + h - 22, w, 22, RGB(228, 230, 238));
    int r, c;
    n_pos(ncur, &r, &c);
    if (r < ntop) ntop = r;
    if (r >= ntop + nrows) ntop = r - nrows + 1;
    int rr = 0, cc = 0;
    for (int i = 0; i < nlen; i++) {
        if (cc >= ncols) { rr++; cc = 0; }
        if (nt[i] == '\n') { rr++; cc = 0; continue; }
        if (rr >= ntop && rr < ntop + nrows)
            gx_char(x + 8 + cc * 8, y + 6 + (rr - ntop) * 16, (unsigned char)nt[i], RGB(30, 32, 44), 1);
        cc++;
    }
    gx_fill(x + 8 + c * 8, y + 6 + (r - ntop) * 16 + 1, 2, 14, RGB(60, 90, 220));
    char b[40], n[12];
    kstrcpy(b, "Ln "); kitoa(r + 1, n); kstrcpy(b + 3, n);
    int l = kstrlen(b); kstrcpy(b + l, "  Col "); kitoa(c + 1, n); kstrcpy(b + l + 6, n);
    gx_text(x + 8, y + h - 19, b, RGB(70, 74, 96), 1);
    { char f[80]; const char *bn = nfile; for (const char *q = nfile; *q; q++) if (*q == '/') bn = q + 1; kstrcpy(f, bn); if (nmsg[0]) { int l = kstrlen(f); kstrcpy(f + l, " - "); kstrcpy(f + l + 3, nmsg); }
      gx_text(x + 160, y + h - 19, f, RGB(60, 90, 220), 1); }
    kitoa(nlen, n); kstrcpy(b, n); l = kstrlen(b); kstrcpy(b + l, " caracteres");
    gx_text(x + w - 8 - gx_textw(b, 1), y + h - 19, b, RGB(70, 74, 96), 1);
}

/* ====================================================================
 *  PAINT
 * ==================================================================== */
static const uint32_t PAL[16] = {
    0x000000, 0x4a4a4a, 0xe5484d, 0xf5842b, 0xf7d02c, 0x46c35f, 0x14a39a, 0x32c5ff,
    0x2f6bff, 0x7b4dff, 0xe052c8, 0x8a5a3c, 0xa0a0a0, 0xd8d8d8, 0xffe9c4, 0xffffff };
static uint32_t *pcv;
static uint8_t *imgbuf, *encbuf;
static int pcw, pch, pcol = 0, pbrush = 3, plx = -1, ply = -1, imgcap, enccap;
static char pfile[60], pmsg[32], homepath[40] = "/";
#define PTOOL 64

static void ui_btn(int x, int y, int w, const char *l, uint32_t bg) {
    gx_rrect(x, y, w, 24, 5, bg);
    gx_text(x + (w - gx_textw(l, 1)) / 2, y + 4, l, RGB(255, 255, 255), 1);
}
static void p_clear(void) { for (int i = 0; i < pcw * pch; i++) pcv[i] = 0xffffff; }
static void p_stamp(int cx, int cy) {
    for (int j = -pbrush; j <= pbrush; j++)
        for (int i = -pbrush; i <= pbrush; i++) {
            if (i * i + j * j > pbrush * pbrush + pbrush) continue;
            int px = cx + i, py = cy + j;
            if (px >= 0 && py >= 0 && px < pcw && py < pch) pcv[py * pcw + px] = PAL[pcol];
        }
}
static void p_line(int x0, int y0, int x1, int y1) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        p_stamp(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* Formato .aimg: "AI", largura(16), altura(16), pares (contagem, cor 0..15) - RLE com a paleta fixa */
static int img_encode(const uint8_t *idx, int w, int h, uint8_t *out) {
    out[0] = 'A'; out[1] = 'I'; out[2] = (uint8_t)w; out[3] = (uint8_t)(w >> 8); out[4] = (uint8_t)h; out[5] = (uint8_t)(h >> 8);
    int o = 6, n = w * h, i = 0;
    while (i < n) {
        uint8_t c = idx[i]; int r = 1;
        while (i + r < n && idx[i + r] == c && r < 255) r++;
        out[o++] = (uint8_t)r; out[o++] = c; i += r;
    }
    return o;
}
static int img_decode(const uint8_t *s, int len, uint8_t *idx, int cap, int *w, int *h) {
    if (len < 6 || s[0] != 'A' || s[1] != 'I') return -1;
    int W = s[2] | (s[3] << 8), H = s[4] | (s[5] << 8);
    if (W <= 0 || H <= 0 || W * H > cap) return -1;
    int n = W * H, i = 0;
    for (int o = 6; o + 1 < len && i < n; o += 2) { int r = s[o]; uint8_t c = s[o + 1] & 15; while (r-- > 0 && i < n) idx[i++] = c; }
    while (i < n) idx[i++] = 15;
    *w = W; *h = H;
    return 0;
}
static int img_load(int id, int *w, int *h) {         /* descodifica para imgbuf */
    if (!imgbuf || !fs_valid(id) || fs_isdir(id)) return -1;
    int n = fs_size(id);
    if (n > enccap) return -1;
    fs_rd(id, encbuf, n);
    return img_decode(encbuf, n, imgbuf, imgcap, w, h);
}
static uint8_t p_idx(uint32_t c) { for (int i = 0; i < 16; i++) if (PAL[i] == c) return (uint8_t)i; return 15; }

void *app_scratch(int n) { return (encbuf && n <= enccap) ? (void *)encbuf : (void *)0; }

static void p_save(void) {
    for (int i = 0; i < pcw * pch; i++) imgbuf[i] = p_idx(pcv[i]);
    int len = img_encode(imgbuf, pcw, pch, encbuf);
    if (!pfile[0]) {
        char dir[64];
        kstrcpy(dir, kstrcmp(homepath, "/") ? homepath : "");
        kstrcpy(dir + kstrlen(dir), "/imagens");
        if (fs_find(dir) < 0) fs_mkpath(dir, 1);
        for (int n = 1; n < 100; n++) {
            char p[80]; kstrcpy(p, dir); kstrcpy(p + kstrlen(p), "/desenho-");
            int l = kstrlen(p); l += kitoa(n, p + l); kstrcpy(p + l, ".aimg");
            if (fs_find(p) < 0) { kstrcpy(pfile, p); break; }
        }
    }
    if (!pfile[0]) { kstrcpy(pmsg, "sem nomes livres"); return; }
    if (fs_save(pfile, encbuf, len) == 0) {
        char n[12]; kitoa(len, n);
        kstrcpy(pmsg, "guardado ("); kstrcpy(pmsg + 10, n); kstrcpy(pmsg + kstrlen(pmsg), " B)");
    } else kstrcpy(pmsg, "erro ao guardar");
}
void paint_open_file(const char *path) {
    int id = fs_find(path), w, h;
    if (!pcv || id < 0 || img_load(id, &w, &h) < 0) { kstrcpy(pmsg, "imagem inválida"); return; }
    p_clear();
    for (int y = 0; y < h && y < pch; y++)
        for (int x = 0; x < w && x < pcw; x++) pcv[y * pcw + x] = PAL[imgbuf[y * w + x]];
    int i = 0; while (path[i] && i < 59) { pfile[i] = path[i]; i++; } pfile[i] = 0;
    kstrcpy(pmsg, "aberto");
}
static void make_sample(void) {                       /* imagem de exemplo em /imagens */
    const int W = 240, H = 160;
    if (fs_find("/imagens/exemplo.aimg") >= 0 || W * H > imgcap) return;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        uint8_t c = 7;
        int dx = x - 190, dy = y - 40;
        if (dx * dx + dy * dy < 28 * 28) c = 3;
        if (dx * dx + dy * dy < 23 * 23) c = 4;
        int cl[3][3] = { { 50, 32, 14 }, { 66, 28, 11 }, { 130, 50, 12 } };
        for (int k = 0; k < 3; k++) { int ex = x - cl[k][0], ey = (y - cl[k][1]) * 2; if (ex * ex + ey * ey < cl[k][2] * cl[k][2]) c = 15; }
        int hx = x - 190, hy = y - 160; if (hx * hx + hy * hy < 70 * 70) c = 6;
        hx = x - 40; hy = y - 175; if (hx * hx + hy * hy < 75 * 75) c = 6;
        if (y >= 135) c = 5;
        if (y >= 100 && y < 135 && x >= 90 && x < 150) c = 14;
        if (y >= 80 && y < 100) { int half = (y - 78) * 36 / 22; if (x >= 120 - half && x <= 120 + half) c = 2; }
        if (y >= 112 && y < 135 && x >= 112 && x < 128) c = 11;
        if (y >= 106 && y < 120 && ((x >= 95 && x < 108) || (x >= 132 && x < 145))) c = 8;
        imgbuf[y * W + x] = c;
    }
    int len = img_encode(imgbuf, W, H, encbuf);
    fs_save("/imagens/exemplo.aimg", encbuf, len);
}

static void paint_click(int rx, int ry) {
    plx = ply = -1;
    if (ry < 36) {
        for (int i = 0; i < 16; i++)
            if (rx >= 8 + i * 24 && rx < 8 + i * 24 + 20 && ry >= 8 && ry < 28) pcol = i;
        if (ry >= 8 && ry < 28) {
            if (rx >= 402 && rx < 422 && pbrush > 1) pbrush--;
            if (rx >= 450 && rx < 470 && pbrush < 20) pbrush++;
        }
        return;
    }
    if (ry < PTOOL) {
        if (ry >= 40 && ry < 64) {
            if (rx >= 8 && rx < 88) p_save();
            else if (rx >= 96 && rx < 166) { p_clear(); pfile[0] = 0; kstrcpy(pmsg, "novo desenho"); }
            else if (rx >= 174 && rx < 264) gui_open("ficheiros");
        }
        return;
    }
    int cx = rx, cy = ry - PTOOL;
    p_stamp(cx, cy);
    plx = cx; ply = cy;
}
static void paint_drag(int rx, int ry) {
    if (ry < PTOOL && plx < 0) return;
    int cx = rx, cy = ry - PTOOL;
    if (plx >= 0) p_line(plx, ply, cx, cy); else p_stamp(cx, cy);
    plx = cx; ply = cy;
}
static void paint_key(int k) {
    if (k == 19) { p_save(); return; }                  /* Ctrl+S */
    if ((k == '+' || k == '*') && pbrush < 20) pbrush++;
    else if ((k == '-' || k == '_') && pbrush > 1) pbrush--;
    else if (k == 'c' || k == 'C') p_clear();
    else if (k >= '1' && k <= '9') pcol = k - '1';
}
static void paint_draw(int x, int y, int w, int h) {
    gx_fill(x, y + PTOOL, w, h - PTOOL, RGB(110, 114, 134));
    gx_fill(x, y, w, PTOOL, RGB(222, 225, 236));
    for (int i = 0; i < 16; i++) {
        int sx = x + 8 + i * 24;
        gx_fill(sx, y + 8, 20, 20, RGB(40, 44, 60));
        gx_fill(sx + 1, y + 9, 18, 18, PAL[i]);
        if (i == pcol) { gx_rect(sx - 2, y + 6, 24, 24, RGB(47, 107, 255)); gx_rect(sx - 1, y + 7, 22, 22, RGB(47, 107, 255)); }
    }
    char b[24];
    gx_rrect(x + 402, y + 8, 20, 20, 4, RGB(100, 108, 150)); gx_text(x + 406, y + 10, "-", RGB(255, 255, 255), 1);
    kitoa(pbrush, b); gx_text(x + 428, y + 10, b, RGB(40, 44, 60), 1);
    gx_rrect(x + 450, y + 8, 20, 20, 4, RGB(100, 108, 150)); gx_text(x + 456, y + 10, "+", RGB(255, 255, 255), 1);
    ui_btn(x + 8, y + 38, 80, "Guardar", RGB(47, 107, 255));
    ui_btn(x + 96, y + 38, 70, "Novo", RGB(200, 64, 76));
    ui_btn(x + 174, y + 38, 90, "Ficheiros", RGB(120, 100, 200));
    const char *bn = pfile; for (const char *q = pfile; *q; q++) if (*q == '/') bn = q + 1;
    gx_text(x + 276, y + 42, pfile[0] ? bn : "(sem título)", RGB(40, 44, 60), 1);
    if (pmsg[0]) gx_text(x + 276, y + 42 + 0, "", 0, 1);
    if (pmsg[0]) { int tx = x + 276 + (pfile[0] ? kstrlen(bn) : 11) * 8 + 8; gx_text(tx, y + 42, pmsg, RGB(60, 90, 220), 1); }
    for (int j = 0; j < pch && j < h - PTOOL; j++) {
        if (y + PTOOL + j < 0 || y + PTOOL + j >= SH) continue;
        uint32_t *d = back + (y + PTOOL + j) * SW + x;
        for (int i = 0; i < pcw && i < w; i++)
            if (x + i >= 0 && x + i < SW) d[i] = pcv[j * pcw + i];
    }
}

/* ====================================================================
 *  CALCULADORA
 * ==================================================================== */
static int cacc, ccur, cop, cnew = 1, cerr;
static const char *CB[20] = {
    "C", "<", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "+/-", "0", "00", "=" };
static void calc_apply(void) {
    int a = cacc, b = ccur;
    switch (cop) {
    case '+': ccur = a + b; break;
    case '-': ccur = a - b; break;
    case '*': ccur = a * b; break;
    case '/': if (b == 0) { cerr = 1; ccur = 0; } else ccur = a / b; break;
    case '%': if (b == 0) { cerr = 1; ccur = 0; } else ccur = a % b; break;
    }
    cacc = ccur;
}
static void calc_press(const char *l) {
    if (cerr && l[0] != 'C') return;
    char c = l[0];
    if (c >= '0' && c <= '9') {
        if (cnew) { ccur = 0; cnew = 0; }
        if (ccur < 10000000) { ccur = ccur * 10 + (c - '0'); if (l[1] == '0') ccur *= 10; }
    } else if (c == 'C') { cacc = ccur = cop = cerr = 0; cnew = 1; }
    else if (c == '<') { if (!cnew) ccur /= 10; }
    else if (c == '+' && l[1] == '/') { ccur = -ccur; }
    else if (c == '=') { if (cop) { calc_apply(); cop = 0; } cnew = 1; cacc = ccur; }
    else { /* operador */
        if (cop && !cnew) calc_apply();
        cacc = ccur; cop = c; cnew = 1;
    }
}
static void calc_key(int k) {
    char b[2] = { (char)k, 0 };
    if ((k >= '0' && k <= '9') || k == '+' || k == '-' || k == '*' || k == '/' || k == '%') calc_press(b);
    else if (k == '=' || k == '\n') calc_press("=");
    else if (k == '\b') calc_press("<");
    else if (k == 'c' || k == 'C' || k == 27) calc_press("C");
}
static void calc_geom(int w, int h, int *bw, int *bh) {
    *bw = (w - 20 - 18) / 4;
    *bh = (h - 76 - 10 - 24) / 5;
}
static void calc_click(int rx, int ry, int w, int h) {
    int bw, bh; calc_geom(w, h, &bw, &bh);
    for (int i = 0; i < 20; i++) {
        int bx = 10 + (i % 4) * (bw + 6), by = 76 + (i / 4) * (bh + 6);
        if (rx >= bx && rx < bx + bw && ry >= by && ry < by + bh) { calc_press(CB[i]); return; }
    }
}
static void calc_draw(int x, int y, int w, int h) {
    gx_fill(x, y, w, h, RGB(30, 33, 48));
    gx_rrect(x + 10, y + 10, w - 20, 56, 8, RGB(14, 17, 26));
    char s[16];
    if (cerr) kstrcpy(s, "Erro"); else kitoa(ccur, s);
    gx_text(x + w - 20 - gx_textw(s, 2), y + 26, s, RGB(236, 240, 255), 2);
    if (cop) { char o[2] = { (char)cop, 0 }; gx_text(x + 18, y + 14, o, RGB(94, 230, 196), 1); }
    int bw, bh; calc_geom(w, h, &bw, &bh);
    for (int i = 0; i < 20; i++) {
        int bx = x + 10 + (i % 4) * (bw + 6), by = y + 76 + (i / 4) * (bh + 6);
        const char *l = CB[i];
        uint32_t bg = RGB(52, 57, 82), fg = RGB(235, 238, 250);
        if (l[0] == '=') { bg = RGB(47, 107, 255); fg = RGB(255, 255, 255); }
        else if (l[0] == 'C') { bg = RGB(200, 64, 76); }
        else if (l[1] == 0 && (l[0] == '/' || l[0] == '*' || l[0] == '-' || l[0] == '+' || l[0] == '%' || l[0] == '<')) bg = RGB(78, 70, 150);
        gx_rrect(bx, by, bw, bh, 8, bg);
        gx_text(bx + (bw - gx_textw(l, 2)) / 2, by + (bh - 32) / 2, l, fg, 2);
    }
}


/* ====================================================================
 *  EXPLORADOR DE FICHEIROS (pastas, subpastas, imagens do Paint)
 * ==================================================================== */
#define F_TB   36
#define F_PATH 22
#define F_HD   24
#define F_ROW  22
#define F_PV   170
static int fdir, fsel, ftop, finput, ffresh, fclip = -1, fclick_row = -1, fn;
static int fids[68];
static char fbuf[24], fmsg[44];
static uint32_t fclick_ms;
static const char *FB_L[6] = { "Ficheiro", "Pasta", "Abrir", "Renomear", "Apagar", "Mover" };
static const int FB_W[6] = { 76, 60, 60, 84, 66, 66 };
static int pvid = -1, pvver = -1, pvw, pvh;
static uint32_t pvpx[150 * 110];

static void f_refresh(void) {
    if (!fs_isdir(fdir)) fdir = 0;
    if (fclip >= 0 && !fs_valid(fclip)) fclip = -1;
    fn = 0;
    if (fdir) fids[fn++] = -2;                           /* ".." */
    fn += fs_list(fdir, fids + fn, 64);
    if (fsel >= fn) fsel = fn - 1;
    if (fsel < 0) fsel = 0;
}
static int f_sid(void) { return fn ? fids[fsel] : -1; }
static int f_vis(int h) { int v = (h - F_TB - F_PATH - F_HD - 22) / F_ROW; return v < 1 ? 1 : v; }
static void f_scroll(void) {
    int v = f_vis(wch[APP_FILES]);
    if (fsel < ftop) ftop = fsel;
    if (fsel >= ftop + v) ftop = fsel - v + 1;
    if (ftop < 0) ftop = 0;
}
static void f_select_id(int id) {
    for (int i = 0; i < fn; i++) if (fids[i] == id) { fsel = i; break; }
    f_scroll();
}
static void f_open_file(int id) {
    char p[160];
    fs_path(id, p);
    if (has_ext(fs_nm(id), ".aimg")) { paint_open_file(p); gui_open("paint"); }
    else { notes_open_file(p); gui_open("notas"); }
}
static void f_enter(void) {
    int id = f_sid();
    if (id == -2) { int old = fdir; fdir = fs_par(fdir); f_refresh(); f_select_id(old); }
    else if (id >= 0 && fs_isdir(id)) { fdir = id; fsel = 0; ftop = 0; f_refresh(); }
    else if (id >= 0) f_open_file(id);
    fmsg[0] = 0;
}
static void f_up(void) {
    if (!fdir) return;
    int old = fdir; fdir = fs_par(fdir); f_refresh(); f_select_id(old); fmsg[0] = 0;
}
static void f_delete(void) {
    int id = f_sid();
    if (id < 0) { kstrcpy(fmsg, "nada selecionado"); return; }
    int rc = fs_rm(id);
    kstrcpy(fmsg, rc == 0 ? "apagado" : rc == -2 ? "a pasta não está vazia" : "erro");
    if (fclip == id) fclip = -1;
    f_refresh(); f_scroll();
}
static void f_begin(int mode) {                          /* 1 ficheiro, 2 pasta, 3 renomear */
    int id = f_sid();
    if (mode == 3 && id < 0) { kstrcpy(fmsg, "nada selecionado"); return; }
    finput = mode; fbuf[0] = 0; fmsg[0] = 0;
    if (mode == 3) { kstrcpy(fbuf, fs_nm(id)); ffresh = 1; }
}
static void f_commit(void) {
    if (!fbuf[0]) { finput = 0; return; }
    if (finput == 3) {
        int id = f_sid();
        if (fs_ren(id, fbuf) == 0) { kstrcpy(fmsg, "renomeado"); f_refresh(); f_select_id(id); }
        else kstrcpy(fmsg, "nome inválido ou já existe");
    } else {
        int id = fs_mk(fdir, fbuf, finput == 2);
        if (id < 0) kstrcpy(fmsg, "nome inválido ou já existe");
        else { f_refresh(); f_select_id(id); kstrcpy(fmsg, finput == 2 ? "pasta criada" : "ficheiro criado"); if (finput == 1) { finput = 0; f_open_file(id); } }
    }
    finput = 0;
}
static void f_move(void) {
    if (fclip < 0) {
        int id = f_sid();
        if (id < 0) { kstrcpy(fmsg, "nada selecionado"); return; }
        fclip = id; kstrcpy(fmsg, "vai à pasta destino e clica Colar");
    } else {
        if (fs_move(fclip, fdir) == 0) kstrcpy(fmsg, "movido"); else kstrcpy(fmsg, "não foi possível mover");
        fclip = -1; f_refresh();
    }
}
static void files_key(int k) {
    f_refresh();
    if (finput) {
        int n = kstrlen(fbuf);
        if (k == '\n') f_commit();
        else if (k == 27) finput = 0;
        else if (k == '\b') { ffresh = 0; if (n > 0) fbuf[n - 1] = 0; }
        else if (k > 32 && k < 256 && k != 127 && k != '/' && (n < 19 || ffresh)) {
            if (ffresh) { fbuf[0] = 0; n = 0; ffresh = 0; }
            fbuf[n] = (char)k; fbuf[n + 1] = 0;
        }
        return;
    }
    if (k == K_UP && fsel > 0) fsel--;
    else if (k == K_DOWN && fsel < fn - 1) fsel++;
    else if (k == K_HOME) fsel = 0;
    else if (k == K_END) fsel = fn - 1;
    else if (k == '\n') f_enter();
    else if (k == '\b' || k == K_LEFT) f_up();
    else if (k == K_RIGHT) f_enter();
    else if (k == K_DEL) f_delete();
    else if (k == 'n' || k == 'N') f_begin(1);
    else if (k == 'p' || k == 'P') f_begin(2);
    else if (k == 'r' || k == 'R') f_begin(3);
    else if (k == 'm' || k == 'M') f_move();
    f_scroll();
}
static void files_click(int rx, int ry) {
    f_refresh();
    if (finput) return;
    int w = wcw[APP_FILES];
    if (ry >= 6 && ry < 30) {
        int bx = 8;
        for (int i = 0; i < 6; i++) {
            if (rx >= bx && rx < bx + FB_W[i]) {
                if (i == 0) f_begin(1); else if (i == 1) f_begin(2); else if (i == 2) f_enter();
                else if (i == 3) f_begin(3); else if (i == 4) f_delete(); else f_move();
                return;
            }
            bx += FB_W[i] + 6;
        }
        return;
    }
    if (ry >= F_TB && ry < F_TB + F_PATH) { if (rx < 34) f_up(); return; }
    int top = F_TB + F_PATH + F_HD;
    if (ry < top || rx >= w - F_PV) return;
    int row = ftop + (ry - top) / F_ROW;
    if (row >= fn) return;
    uint32_t t = hal_ms();
    if (row == fclick_row && t - fclick_ms < 450) { fsel = row; f_enter(); fclick_row = -1; return; }
    fsel = row; fclick_row = row; fclick_ms = t; fmsg[0] = 0;
}
static void f_thumb(int id) {
    if (id == pvid && fs_ver() == pvver) return;
    pvid = id; pvver = fs_ver(); pvw = pvh = 0;
    int w, h;
    if (img_load(id, &w, &h) < 0) return;
    int tw, th;
    if (w * 110 > h * 150) { tw = 150; th = h * 150 / w; } else { th = 110; tw = w * 110 / h; }
    if (tw < 1) tw = 1;
    if (th < 1) th = 1;
    for (int y = 0; y < th; y++) for (int x = 0; x < tw; x++) pvpx[y * tw + x] = PAL[imgbuf[(y * h / th) * w + x * w / tw]];
    pvw = tw; pvh = th;
}
static void f_sizestr(int len, char *b) {
    char t[12];
    if (len < 1024) { kitoa(len, t); kstrcpy(b, t); kstrcpy(b + kstrlen(b), " B"); }
    else { kitoa((len + 1023) / 1024, t); kstrcpy(b, t); kstrcpy(b + kstrlen(b), " KB"); }
}
static void f_icon(int x, int y, int id, int sel) {
    if (id == -2 || fs_isdir(id)) {
        gx_rrect(x, y + 3, 18, 14, 3, RGB(236, 180, 60));
        gx_rrect(x, y + 1, 8, 5, 2, RGB(214, 150, 40));
        gx_rrect(x, y + 6, 18, 11, 3, RGB(250, 210, 100));
    } else if (has_ext(fs_nm(id), ".aimg")) {
        gx_fill(x + 1, y + 2, 16, 14, RGB(50, 197, 255));
        gx_fill(x + 1, y + 11, 16, 5, RGB(70, 195, 95));
        gx_circle(x + 12, y + 6, 2, RGB(247, 208, 44));
        gx_rect(x + 1, y + 2, 16, 14, sel ? RGB(255, 255, 255) : RGB(60, 70, 120));
    } else {
        gx_rrect(x + 2, y + 1, 14, 16, 2, sel ? RGB(255, 255, 255) : RGB(250, 250, 246));
        if (!sel) gx_rect(x + 2, y + 1, 14, 16, RGB(150, 160, 200));
        for (int k = 0; k < 3; k++) gx_fill(x + 5, y + 5 + k * 4, 8, 1, RGB(110, 130, 210));
    }
}
static void files_draw(int x, int y, int w, int h) {
    f_refresh(); f_scroll();
    int lw = w - F_PV;
    gx_fill(x, y, w, h, RGB(248, 249, 252));
    gx_fill(x, y, w, F_TB, RGB(222, 225, 236));
    int bx = x + 8;
    static const uint32_t FC[6] = { 0x2f6bff, 0x3c966e, 0x3c966e, 0x7864c8, 0xc8404c, 0xd9822b };
    for (int i = 0; i < 6; i++) {
        ui_btn(bx, y + 6, FB_W[i], i == 5 && fclip >= 0 ? "Colar" : FB_L[i], FC[i]);
        bx += FB_W[i] + 6;
    }
    /* barra de caminho */
    gx_fill(x, y + F_TB, w, F_PATH, RGB(240, 242, 250));
    gx_rrect(x + 8, y + F_TB + 2, 22, 18, 4, fdir ? RGB(100, 108, 150) : RGB(180, 184, 205));
    gx_text(x + 15, y + F_TB + 3, "^", RGB(255, 255, 255), 1);
    char path[160]; fs_path(fdir, path);
    gx_text(x + 40, y + F_TB + 3, path, RGB(50, 56, 90), 1);
    gx_fill(x, y + F_TB, w, 1, RGB(206, 210, 228));
    /* cabecalho */
    int hy = y + F_TB + F_PATH;
    gx_fill(x, hy, lw, F_HD, RGB(232, 234, 244));
    gx_text(x + 40, hy + 4, "Nome", RGB(70, 76, 110), 1);
    gx_text(x + lw - 16 - gx_textw("Tamanho", 1), hy + 4, "Tamanho", RGB(70, 76, 110), 1);
    int vis = f_vis(h);
    for (int r = ftop; r < fn && r < ftop + vis; r++) {
        int id = fids[r], ry = hy + F_HD + (r - ftop) * F_ROW, sel = r == fsel;
        gx_fill(x, ry, lw, F_ROW, sel ? RGB(60, 96, 220) : (r & 1) ? RGB(240, 242, 249) : RGB(248, 249, 252));
        f_icon(x + 12, ry + 2, id, sel);
        const char *nm = id == -2 ? ".." : fs_nm(id);
        uint32_t fg = sel ? RGB(255, 255, 255) : RGB(30, 32, 44);
        int tx = gx_text(x + 40, ry + 3, nm, fg, 1);
        if (id >= 0 && fs_isdir(id)) tx = gx_text(tx, ry + 3, "/", fg, 1);
        if (id == fclip && id >= 0) gx_text(tx + 8, ry + 3, "(a mover)", sel ? RGB(255, 220, 160) : RGB(217, 130, 43), 1);
        char b[24];
        if (id == -2) b[0] = 0; else if (fs_isdir(id)) kstrcpy(b, "pasta"); else f_sizestr(fs_size(id), b);
        gx_text(x + lw - 16 - gx_textw(b, 1), ry + 3, b, sel ? RGB(230, 236, 255) : RGB(90, 96, 120), 1);
    }
    if (fn == 0) gx_text(x + 40, hy + F_HD + 8, "(pasta vazia)", RGB(120, 126, 150), 1);
    if (fn > vis) {                                      /* barra de scroll */
        int th = h - F_TB - F_PATH - F_HD - 22;
        gx_fill(x + lw - 6, hy + F_HD, 6, th, RGB(226, 228, 240));
        int bh = th * vis / fn; if (bh < 14) bh = 14;
        gx_rrect(x + lw - 6, hy + F_HD + (th - bh) * ftop / (fn - vis), 6, bh, 3, RGB(120, 130, 180));
    }
    /* painel de pre-visualizacao */
    int px = x + lw;
    gx_fill(px, hy, F_PV, h - F_TB - F_PATH - 22, RGB(236, 238, 246));
    gx_fill(px, hy, 1, h - F_TB - F_PATH - 22, RGB(206, 210, 228));
    gx_text(px + 10, hy + 4, "Pré-visualização", RGB(70, 76, 110), 1);
    int id = f_sid();
    if (id >= 0) {
        gx_textb(px + 10, hy + 30, fs_nm(id), RGB(30, 32, 44));
        if (fs_isdir(id)) {
            int t[64], n = fs_list(id, t, 64); char b[20];
            kitoa(n, b); kstrcpy(b + kstrlen(b), n == 1 ? " item" : " itens");
            gx_text(px + 10, hy + 54, b, RGB(90, 96, 120), 1);
        } else if (has_ext(fs_nm(id), ".aimg")) {
            f_thumb(id);
            if (pvw) {
                int ox = px + (F_PV - pvw) / 2, oy = hy + 56;
                gx_rect(ox - 1, oy - 1, pvw + 2, pvh + 2, RGB(90, 96, 130));
                for (int j = 0; j < pvh; j++) for (int i = 0; i < pvw; i++) gx_px(ox + i, oy + j, pvpx[j * pvw + i]);
            } else gx_text(px + 10, hy + 54, "(imagem inválida)", RGB(200, 64, 76), 1);
            char b[24]; f_sizestr(fs_size(id), b);
            gx_text(px + 10, hy + 54 + 120, b, RGB(90, 96, 120), 1);
        } else {
            char buf[200]; int n = fs_rd(id, buf, 190);
            int line = 0, col = 0; char ln[24];
            for (int i = 0; i <= n && line < 8; i++) {
                int c = i < n ? (unsigned char)buf[i] : '\n';
                if (c == '\n') { ln[col] = 0; gx_text(px + 10, hy + 54 + line * 16, ln, RGB(60, 66, 96), 1); line++; col = 0; }
                else if (c >= 32 && col < 19) ln[col++] = (char)c;
            }
            char b[24]; f_sizestr(fs_size(id), b);
            gx_text(px + 10, hy + 54 + 8 * 16 + 6, b, RGB(120, 126, 150), 1);
        }
    }
    /* rodape */
    gx_fill(x, y + h - 22, w, 22, RGB(228, 230, 238));
    if (finput) {
        gx_fill(x, y + h - 22, w, 22, RGB(255, 252, 224));
        const char *lb = finput == 1 ? "Novo ficheiro: " : finput == 2 ? "Nova pasta: " : "Novo nome: ";
        int tx = gx_text(x + 8, y + h - 19, lb, RGB(70, 74, 96), 1);
        tx = gx_text(tx, y + h - 19, fbuf, RGB(20, 24, 40), 1);
        gx_fill(tx, y + h - 19, 2, 14, RGB(60, 90, 220));
        gx_text(x + w - 8 - gx_textw("Enter ok  Esc cancela", 1), y + h - 19, "Enter ok  Esc cancela", RGB(120, 124, 150), 1);
    } else {
        char b[48], t[12]; int cnt = fn - (fdir ? 1 : 0);
        kitoa(cnt, t); kstrcpy(b, t); kstrcpy(b + kstrlen(b), cnt == 1 ? " item   " : " itens   ");
        kitoa(fs_used() / 1024, t); kstrcpy(b + kstrlen(b), t); kstrcpy(b + kstrlen(b), " KB usados");
        gx_text(x + 8, y + h - 19, b, RGB(70, 74, 96), 1);
        if (fmsg[0]) gx_text(x + w - 8 - gx_textw(fmsg, 1), y + h - 19, fmsg, RGB(60, 90, 220), 1);
    }
}

/* ====================================================================
 *  MEMORIA (.aum)
 * ==================================================================== */
static AumInfo mi;
static char mmsg[80];
static int mmsg_ok = 1;

void mem_set_msg(const char *m, int ok) { int i = 0; while (m[i] && i < 78) { mmsg[i] = m[i]; i++; } mmsg[i] = 0; mmsg_ok = ok; }
const char *mem_msg(void) { return mmsg; }
static void mem_refresh(void) { aum_probe(&mi); }
static void cat_int(char *b, int v) { char t[12]; kitoa(v, t); kstrcpy(b + kstrlen(b), t); }
int mem_do_save(void) {
    int need = 0, rc = aum_save(&need);
    char b[80];
    if (rc == 0) {
        kstrcpy(b, "Guardado: "); cat_int(b, fs_count()); kstrcpy(b + kstrlen(b), " itens, ");
        cat_int(b, (fs_used() + 1023) / 1024); kstrcpy(b + kstrlen(b), " KB de dados.");
        mem_set_msg(b, 1);
    } else if (rc == -1) mem_set_msg("Nenhum disco .aum ligado (ver ajuda abaixo).", 0);
    else if (rc == -2) { kstrcpy(b, "Disco pequeno demais: precisa de "); cat_int(b, need); kstrcpy(b + kstrlen(b), " KB."); mem_set_msg(b, 0); }
    else mem_set_msg("Erro ao escrever no disco.", 0);
    mem_refresh();
    return rc;
}
void mem_do_load(void) {
    if (users_cur() >= 0 && !users_admin(users_cur())) { mem_set_msg("Apenas administradores podem carregar a memória.", 0); return; }
    int rc = aum_load();
    if (rc == 0) {
        char b[80];
        kstrcpy(b, "Memória carregada: "); cat_int(b, fs_count()); kstrcpy(b + kstrlen(b), " itens.");
        mem_set_msg(b, 1);
    } else if (rc == -1) mem_set_msg("Não há nenhuma memória .aum válida no disco.", 0);
    else mem_set_msg("O .aum está corrompido; foi ignorado (CRC inválido).", 0);
    mem_refresh();
}
void mem_do_auto(int on) {
    int rc = aum_set_auto(on);
    if (rc == 0) mem_set_msg(on ? "Memória interna ligada: tudo é guardado sozinho." : "Memória interna desligada (modo manual).", 1);
    else if (rc == -1) mem_set_msg("Nenhum disco ligado: não há onde guardar.", 0);
    else mem_set_msg("Erro ao escrever no disco.", 0);
    mem_refresh();
}
static void mem_key(int k) {
    if (k == 19 || k == 's' || k == 'S' || k == 'g' || k == 'G') mem_do_save();
    else if (k == 'c' || k == 'C' || k == 'l' || k == 'L') gui_request_load();
    else if (k == 'r' || k == 'R') { mem_refresh(); mem_set_msg("Disco relido.", 1); }
    else if (k == 'a' || k == 'A') gui_request_auto(!aum_auto_flag());
}
static void mem_click(int rx, int ry) {
    if (ry >= 150 && ry < 176 && rx >= 20 && rx < 400) { gui_request_auto(!aum_auto_flag()); return; }
    if (ry >= 188 && ry < 212) {
        if (rx >= 20 && rx < 190) mem_do_save();
        else if (rx >= 200 && rx < 370) gui_request_load();
        else if (rx >= 380 && rx < 480) { mem_refresh(); mem_set_msg("Disco relido.", 1); }
    }
}
static void mem_draw(int x, int y, int w, int h) {
    gx_vgrad(x, y, w, h, RGB(24, 28, 52), RGB(18, 20, 36));
    gx_rrect(x + 20, y + 16, 44, 44, 6, RGB(94, 230, 196));
    gx_rrect(x + 27, y + 23, 30, 30, 3, RGB(34, 40, 90));
    for (int i = 0; i < 4; i++) { gx_fill(x + 27 + i * 8, y + 11, 3, 5, RGB(94, 230, 196)); gx_fill(x + 27 + i * 8, y + 60, 3, 5, RGB(94, 230, 196)); }
    gx_text(x + 36, y + 28, "AUM", RGB(255, 255, 255), 1);
    gx_text(x + 80, y + 18, "Memória (.aum)", RGB(255, 255, 255), 2);
    gx_text(x + 80, y + 50, "guarda e carrega tudo num disco .aum", RGB(160, 170, 210), 1);
    uint32_t lb = RGB(120, 132, 190), ok = RGB(94, 230, 196), warn = RGB(247, 170, 60), bad = RGB(240, 100, 110), tx = RGB(232, 236, 250);
    char b[96];
    int ty = y + 84;
    gx_text(x + 20, ty, "Disco .aum", lb, 1);
    if (mi.state == AUM_NODISK) {
        gx_text(x + 140, ty, mi.foreign ? "só há discos que não são .aum (ignorados)" : "nenhum ligado", mi.foreign ? warn : bad, 1);
    } else {
        kstrcpy(b, mi.state == AUM_BLANK ? "novo (vazio), " : mi.state == AUM_VALID ? "detetado, " : "cabeçalho inválido, ");
        cat_int(b, (int)(mi.sectors / 2)); kstrcpy(b + kstrlen(b), " KB");
        gx_text(x + 140, ty, b, mi.state == AUM_BAD ? warn : ok, 1);
    }
    ty += 24;
    gx_text(x + 20, ty, "No disco", lb, 1);
    if (mi.state == AUM_VALID) {
        char t[12];
        kpad2(b, mi.d); b[2] = '/'; kpad2(b + 3, mi.mo); b[5] = '/'; kitoa(mi.y, b + 6);
        kstrcpy(b + kstrlen(b), "  "); kpad2(t, mi.h); t[2] = ':'; kpad2(t + 3, mi.mi); t[5] = 0; kstrcpy(b + kstrlen(b), t);
        kstrcpy(b + kstrlen(b), "  -  "); cat_int(b, mi.count); kstrcpy(b + kstrlen(b), " itens, ");
        cat_int(b, (int)((mi.payload + 1023) / 1024)); kstrcpy(b + kstrlen(b), " KB");
        if (mi.ver >= 2) { kstrcpy(b + kstrlen(b), ", "); cat_int(b, mi.users); kstrcpy(b + kstrlen(b), mi.users == 1 ? " utilizador" : " utilizadores"); }
        gx_text(x + 140, ty, b, tx, 1);
    } else gx_text(x + 140, ty, "-", RGB(120, 126, 150), 1);
    ty += 24;
    gx_text(x + 20, ty, "Na RAM", lb, 1);
    kstrcpy(b, ""); cat_int(b, fs_count()); kstrcpy(b + kstrlen(b), " itens, "); cat_int(b, (fs_used() + 1023) / 1024); kstrcpy(b + kstrlen(b), " KB");
    int ex = gx_text(x + 140, ty, b, tx, 1);
    if (aum_dirty()) gx_text(ex + 16, ty, "* por guardar", warn, 1);
    {   /* interruptor: memoria interna automatica */
        int on = aum_auto_flag(), sy = y + 152;
        gx_text(x + 20, sy + 4, "Memória interna", lb, 1);
        gx_rrect(x + 160, sy, 46, 22, 11, on ? RGB(60, 190, 130) : RGB(70, 76, 110));
        gx_circle(x + (on ? 195 : 171), sy + 11, 8, RGB(255, 255, 255));
        const char *st = !on ? "desligada (modo manual)" : aum_auto_active() ? "ligada: carrega e guarda sozinha" : "suspensa (houve um erro)";
        gx_text(x + 218, sy + 4, st, !on ? RGB(140, 146, 180) : aum_auto_active() ? ok : bad, 1);
    }
    ui_btn(x + 20, y + 188, 170, "Guardar memória", RGB(47, 107, 255));
    ui_btn(x + 200, y + 188, 170, "Carregar do .aum", RGB(60, 150, 110));
    ui_btn(x + 380, y + 188, 100, "Reler disco", RGB(120, 100, 200));
    if (mmsg[0]) gx_text(x + 20, y + 224, mmsg, mmsg_ok ? ok : bad, 1);
    int hy = y + h - 126;
    gx_rrect(x + 20, hy, w - 40, 110, 8, RGB(14, 17, 28));
    gx_text(x + 32, hy + 10, "Como ligar um .aum (QEMU):", warn, 1);
    gx_text(x + 32, hy + 30, "-drive file=memoria.aum,format=raw,if=ide,index=1", tx, 1);
    gx_text(x + 32, hy + 46, "e arranca a ISO como CD: -boot d   (ou ./run.sh)", tx, 1);
    gx_text(x + 32, hy + 68, "Criar vazio:   python3 tools/aum.py new memoria.aum", RGB(160, 170, 210), 1);
    gx_text(x + 32, hy + 84, "Ver/extrair:   tools/aum.py ls | extract | pack", RGB(160, 170, 210), 1);
}

/* ====================================================================
 *  UTILIZADORES
 * ==================================================================== */
enum { UI_NONE, UI_NEWNAME, UI_NEWPW, UI_NEWPW2, UI_CPOLD, UI_CPNEW, UI_CPNEW2, UI_DEL };
#define U_TOP 64
#define U_ROW 30
static int usel, uinput, umsg_ok = 1;
static char ubuf[32], utmp[32], unew[16], umsg[72];

static void u_msg(const char *m, int ok) { int i = 0; while (m[i] && i < 70) { umsg[i] = m[i]; i++; } umsg[i] = 0; umsg_ok = ok; }
static int u_adm(void) { return users_admin(users_cur()); }
static void u_wipe(void) { for (int i = 0; i < 32; i++) ubuf[i] = utmp[i] = 0; }
static void u_begin(int m) { uinput = m; ubuf[0] = 0; umsg[0] = 0; }
static const char *u_err(int rc) {
    return rc == -1 ? "Nome inválido (1-12 letras, números, - ou _)." : rc == -2 ? "Esse nome já existe." :
           rc == -3 ? "Limite de utilizadores atingido (8)." : "A palavra-passe precisa de pelo menos 4 caracteres.";
}
static void u_do_new(void) {
    if (!u_adm()) { u_msg("Só administradores podem criar utilizadores.", 0); return; }
    u_begin(UI_NEWNAME);
}
static void u_do_chpw(void) {
    if (usel != users_cur() && !u_adm()) { u_msg("Só podes alterar a tua própria palavra-passe.", 0); return; }
    u_begin(usel == users_cur() ? UI_CPOLD : UI_CPNEW);
}
static void u_do_admin(void) {
    if (!u_adm()) { u_msg("Só administradores podem mudar funções.", 0); return; }
    int rc = users_setadmin(usel, !users_admin(usel));
    if (rc == -2) u_msg("Tem de existir pelo menos um administrador.", 0);
    else { u_msg(users_admin(usel) ? "Agora é administrador." : "Agora é utilizador normal.", 1); gui_session_check(); }
}
static void u_do_del(void) {
    if (!u_adm()) { u_msg("Só administradores podem apagar contas.", 0); return; }
    if (usel == users_cur()) { u_msg("Não podes apagar a conta com que estás em sessão.", 0); return; }
    u_begin(UI_DEL);
}
static void u_commit(void) {
    int n = kstrlen(ubuf), rc;
    switch (uinput) {
    case UI_NEWNAME:
        if (!users_valid_name(ubuf)) { u_msg(u_err(-1), 0); uinput = 0; }
        else if (users_find(ubuf) >= 0) { u_msg(u_err(-2), 0); uinput = 0; }
        else { kstrcpy(unew, ubuf); uinput = UI_NEWPW; ubuf[0] = 0; }
        return;
    case UI_NEWPW: case UI_CPNEW:
        if (n < 4) { u_msg(u_err(-4), 0); uinput = 0; u_wipe(); return; }
        kstrcpy(utmp, ubuf); uinput = uinput == UI_NEWPW ? UI_NEWPW2 : UI_CPNEW2; ubuf[0] = 0;
        return;
    case UI_NEWPW2:
        if (kstrcmp(ubuf, utmp)) u_msg("As palavras-passe não coincidem.", 0);
        else if ((rc = users_add(unew, utmp, 0)) != 0) u_msg(u_err(rc), 0);
        else { char b[60]; kstrcpy(b, "Utilizador '"); kstrcpy(b + kstrlen(b), unew); kstrcpy(b + kstrlen(b), "' criado."); u_msg(b, 1); usel = users_find(unew); }
        break;
    case UI_CPOLD:
        if (users_check(usel, ubuf)) { uinput = UI_CPNEW; ubuf[0] = 0; return; }
        u_msg("Palavra-passe atual errada.", 0);
        break;
    case UI_CPNEW2:
        if (kstrcmp(ubuf, utmp)) u_msg("As palavras-passe não coincidem.", 0);
        else if ((rc = users_setpw(usel, utmp)) != 0) u_msg(u_err(rc), 0);
        else u_msg("Palavra-passe alterada.", 1);
        break;
    case UI_DEL:
        if (kstrcmp(ubuf, users_name(usel))) u_msg("Nome não coincide: nada foi apagado.", 0);
        else if ((rc = users_del(usel)) == -2) u_msg("Tem de existir pelo menos um administrador.", 0);
        else { u_msg("Conta apagada (a pasta pessoal fica guardada).", 1); if (usel >= users_n()) usel = users_n() - 1; gui_session_check(); }
        break;
    }
    uinput = 0; u_wipe();
}
static void users_key(int k) {
    if (uinput) {
        int n = kstrlen(ubuf);
        if (k == '\n') u_commit();
        else if (k == 27) { uinput = 0; u_wipe(); }
        else if (k == '\b') { if (n > 0) ubuf[n - 1] = 0; }
        else if (k >= 32 && k < 256 && k != 127 && n < 30 && (uinput != UI_NEWNAME || (k > 32 && n < 12)) && (k != ' ' || (uinput != UI_DEL))) { ubuf[n] = (char)k; ubuf[n + 1] = 0; }
        return;
    }
    if (k == K_UP && usel > 0) usel--;
    else if (k == K_DOWN && usel < users_n() - 1) usel++;
    else if (k == 'n' || k == 'N') u_do_new();
    else if (k == 'p' || k == 'P') u_do_chpw();
    else if (k == 'a' || k == 'A') u_do_admin();
    else if (k == K_DEL) u_do_del();
}
static void users_click(int rx, int ry) {
    int h = wch[APP_USERS];
    if (uinput) return;
    if (ry >= U_TOP && ry < U_TOP + U_ROW * users_n()) { usel = (ry - U_TOP) / U_ROW; return; }
    if (ry >= h - 78 && ry < h - 54) {
        if (rx >= 8 && rx < 88) u_do_new();
        else if (rx >= 96 && rx < 236) u_do_chpw();
        else if (rx >= 244 && rx < 344) u_do_admin();
        else if (rx >= 352 && rx < 432) u_do_del();
    }
}
static void users_draw(int x, int y, int w, int h) {
    if (usel >= users_n()) usel = users_n() - 1;
    if (usel < 0) usel = 0;
    gx_fill(x, y, w, h, RGB(248, 249, 252));
    gx_vgrad(x, y, w, 52, RGB(40, 48, 96), RGB(30, 36, 76));
    gx_circle(x + 30, y + 26, 16, RGB(94, 230, 196));
    gx_circle(x + 30, y + 21, 5, RGB(30, 36, 76)); gx_rrect(x + 21, y + 28, 18, 9, 4, RGB(30, 36, 76));
    gx_text(x + 58, y + 10, "Utilizadores", RGB(255, 255, 255), 2);
    char b[40]; kstrcpy(b, "Sessão: "); kstrcpy(b + 8, users_name(users_cur()));
    gx_text(x + w - 16 - gx_textw(b, 1), y + 18, b, RGB(190, 200, 240), 1);
    for (int i = 0; i < users_n(); i++) {
        int ry = y + U_TOP + i * U_ROW, sel = i == usel;
        gx_fill(x, ry, w, U_ROW, sel ? RGB(60, 96, 220) : (i & 1) ? RGB(240, 242, 249) : RGB(248, 249, 252));
        gx_circle(x + 26, ry + U_ROW / 2, 11, users_color(i));
        char c0 = users_name(i)[0];
        char ini[2] = { (c0 >= 'a' && c0 <= 'z') ? (char)(c0 - 32) : c0, 0 };
        gx_text(x + 22, ry + 7, ini, RGB(255, 255, 255), 1);
        int tx = gx_text(x + 48, ry + 7, users_name(i), sel ? RGB(255, 255, 255) : RGB(30, 32, 44), 1);
        if (i == users_cur()) gx_text(tx + 8, ry + 7, "(tu)", sel ? RGB(255, 230, 170) : RGB(217, 130, 43), 1);
        const char *role = users_admin(i) ? "administrador" : "utilizador";
        gx_text(x + w - 16 - gx_textw(role, 1), ry + 7, role, users_admin(i) ? (sel ? RGB(170, 255, 230) : RGB(20, 150, 120)) : (sel ? RGB(220, 226, 255) : RGB(110, 116, 140)), 1);
    }
    int adm = u_adm(), by = y + h - 78;
    ui_btn(x + 8, by, 80, "Novo", adm ? RGB(47, 107, 255) : RGB(170, 174, 196));
    ui_btn(x + 96, by, 140, "Palavra-passe", (usel == users_cur() || adm) ? RGB(120, 100, 200) : RGB(170, 174, 196));
    ui_btn(x + 244, by, 100, users_admin(usel) ? "Tirar admin" : "Dar admin", adm ? RGB(217, 130, 43) : RGB(170, 174, 196));
    ui_btn(x + 352, by, 80, "Apagar", adm ? RGB(200, 64, 76) : RGB(170, 174, 196));
    if (umsg[0]) gx_text(x + 10, y + h - 44, umsg, umsg_ok ? RGB(20, 150, 100) : RGB(200, 64, 76), 1);
    gx_fill(x, y + h - 24, w, 24, uinput ? RGB(255, 252, 224) : RGB(228, 230, 238));
    if (uinput) {
        static const char *lbl[8] = { "", "Nome do novo utilizador: ", "Palavra-passe: ", "Repetir palavra-passe: ", "Palavra-passe atual: ",
                                      "Nova palavra-passe: ", "Repetir nova palavra-passe: ", "Escreve o nome para confirmar: " };
        int tx = gx_text(x + 8, y + h - 20, lbl[uinput], RGB(70, 74, 96), 1);
        if (uinput == UI_NEWNAME || uinput == UI_DEL) tx = gx_text(tx, y + h - 20, ubuf, RGB(20, 24, 40), 1);
        else for (int i = 0; ubuf[i]; i++) tx = gx_text(tx, y + h - 20, "*", RGB(20, 24, 40), 1);
        gx_fill(tx, y + h - 20, 2, 14, RGB(60, 90, 220));
    } else gx_text(x + 8, y + h - 20, "n novo   p palavra-passe   a admin   Del apagar", RGB(110, 116, 140), 1);
}

/* ====================================================================
 *  SNAKE
 * ==================================================================== */
#define SG_W 24
#define SG_H 18
#define SG_C 20
#define SN_HUD 28
static int snx[SG_W * SG_H], sny[SG_W * SG_H];
static int snlen, sndir, snnd, snfx, snfy, snscore, snhi, snalive = 1, snpause;
static uint32_t sntime, srnd = 2463534242u;
static int sn_rnd(int n) { srnd ^= srnd << 13; srnd ^= srnd >> 17; srnd ^= srnd << 5; return (int)(srnd % (uint32_t)n); }
static void sn_food(void) {
    for (;;) {
        snfx = sn_rnd(SG_W); snfy = sn_rnd(SG_H);
        int ok = 1;
        for (int i = 0; i < snlen; i++) if (snx[i] == snfx && sny[i] == snfy) { ok = 0; break; }
        if (ok) return;
    }
}
static void sn_reset(void) {
    snlen = 4;
    for (int i = 0; i < snlen; i++) { snx[i] = 10 - i; sny[i] = 9; }
    sndir = snnd = 0; snscore = 0; snalive = 1; snpause = 0;
    sn_food();
}
static void sn_step(void) {
    static const int dx[4] = { 1, 0, -1, 0 }, dy[4] = { 0, 1, 0, -1 };
    sndir = snnd;
    int nx = snx[0] + dx[sndir], ny = sny[0] + dy[sndir];
    if (nx < 0 || ny < 0 || nx >= SG_W || ny >= SG_H) { snalive = 0; return; }
    for (int i = 0; i < snlen - 1; i++) if (snx[i] == nx && sny[i] == ny) { snalive = 0; return; }
    int eat = nx == snfx && ny == snfy;
    if (eat) snlen++;
    for (int i = snlen - 1; i > 0; i--) { snx[i] = snx[i - 1]; sny[i] = sny[i - 1]; }
    snx[0] = nx; sny[0] = ny;
    if (eat) {
        snscore += 10; if (snscore > snhi) snhi = snscore;
        if (snlen >= SG_W * SG_H) { snalive = 0; return; }
        sn_food();
    }
}
static void snake_key(int k) {
    int d = -1;
    if (k == K_RIGHT || k == 'd' || k == 'D') d = 0;
    else if (k == K_DOWN || k == 's' || k == 'S') d = 1;
    else if (k == K_LEFT || k == 'a' || k == 'A') d = 2;
    else if (k == K_UP || k == 'w' || k == 'W') d = 3;
    if (d >= 0) { if ((d ^ 2) != sndir) snnd = d; snpause = 0; }
    else if (k == ' ' || k == '\n') { if (!snalive) sn_reset(); else snpause = !snpause; }
}
static int snake_tick(uint32_t ms) {
    if (!snalive || snpause) return 0;
    int period = 150 - snscore / 2; if (period < 60) period = 60;
    if (ms - sntime < (uint32_t)period) return 0;
    sntime = ms; sn_step();
    return 1;
}
static void snake_draw(int x, int y, int w, int h) {
    gx_fill(x, y, w, h, RGB(18, 22, 34));
    gx_fill(x, y, w, SN_HUD, RGB(30, 36, 56));
    char b[40], n[12];
    kstrcpy(b, "Pontos: "); kitoa(snscore, n); kstrcpy(b + 8, n);
    gx_text(x + 10, y + 6, b, RGB(94, 230, 196), 1);
    kstrcpy(b, "Recorde: "); kitoa(snhi, n); kstrcpy(b + 9, n);
    gx_text(x + w - 10 - gx_textw(b, 1), y + 6, b, RGB(247, 208, 44), 1);
    gx_text(x + w / 2 - 36, y + 6, "Snake", RGB(200, 206, 230), 1);
    int bx = x + (w - SG_W * SG_C) / 2, by = y + SN_HUD + (h - SN_HUD - SG_H * SG_C) / 2;
    for (int j = 0; j < SG_H; j++) for (int i = 0; i < SG_W; i++)
        gx_fill(bx + i * SG_C, by + j * SG_C, SG_C, SG_C, ((i + j) & 1) ? RGB(24, 30, 46) : RGB(28, 35, 52));
    gx_circle(bx + snfx * SG_C + SG_C / 2, by + snfy * SG_C + SG_C / 2, 7, RGB(240, 80, 90));
    for (int i = snlen - 1; i >= 0; i--) {
        uint32_t c = i == 0 ? RGB(150, 255, 220) : gx_mix(RGB(70, 200, 120), RGB(20, 110, 100), i * 256 / snlen);
        gx_rrect(bx + snx[i] * SG_C + 1, by + sny[i] * SG_C + 1, SG_C - 2, SG_C - 2, 5, c);
    }
    if (!snalive || snpause) {
        const char *m = !snalive ? "Fim de jogo - Enter para recomeçar" : "Pausa - prime uma tecla";
        int tw = gx_textw(m, 1);
        gx_rrect(x + (w - tw) / 2 - 14, y + h / 2 - 18, tw + 28, 36, 8, RGB(10, 12, 20));
        gx_text(x + (w - tw) / 2, y + h / 2 - 8, m, RGB(255, 255, 255), 1);
    }
}

/* ====================================================================
 *  SOBRE
 * ==================================================================== */
static void about_draw(int x, int y, int w, int h) {
    gx_vgrad(x, y, w, h, RGB(24, 28, 52), RGB(18, 20, 36));
    gx_circle(x + 54, y + 62, 38, RGB(94, 230, 196));
    gx_circle(x + 54, y + 62, 30, RGB(34, 40, 90));
    gx_text(x + 54 - 12, y + 62 - 16, "A", RGB(255, 255, 255), 3);
    gx_text(x + 110, y + 34, "AuroraOS", RGB(255, 255, 255), 2);
    gx_text(x + 110, y + 70, "versão 1.0  -  feito de raiz", RGB(160, 170, 210), 1);
    int ty = y + 118; uint32_t lb = RGB(120, 132, 190), vl = RGB(232, 236, 250);
    gx_text(x + 24, ty, "Bootloader", lb, 1);  gx_text(x + 150, ty, "assembly x86, BIOS + El Torito", vl, 1); ty += 22;
    gx_text(x + 24, ty, "Kernel", lb, 1);      gx_text(x + 150, ty, "C, 32 bits, modo protegido", vl, 1); ty += 22;
    gx_text(x + 24, ty, "Interface", lb, 1);   gx_text(x + 150, ty, "framebuffer VBE + rato PS/2", vl, 1); ty += 22;
    gx_text(x + 24, ty, "Teclado", lb, 1);     gx_text(x + 150, ty, "PS/2, layout português", vl, 1); ty += 22;
    gx_text(x + 24, ty, "Vídeo", lb, 1);       gx_text(x + 150, ty, hal_video(), vl, 1); ty += 22;
    gx_text(x + 24, ty, "CPU", lb, 1);         gx_text(x + 150, ty, hal_cpu(), vl, 1); ty += 34;
    gx_text(x + 24, ty, "Sem Linux, sem libc, sem GRUB.", RGB(94, 230, 196), 1);
}

/* ====================================================================
 *  despacho
 * ==================================================================== */
const char *app_title(int id) {
    static const char *t[NAPPS] = { "Terminal", "Bloco de Notas", "Paint", "Calculadora", "Ficheiros", "Memória (.aum)", "Utilizadores", "Snake", "Sobre o AuroraOS" };
    return t[id];
}
void app_size(int id, int *w, int *h) {
    static const int sz[NAPPS][2] = { { 656, 440 }, { 560, 420 }, { 560, 480 }, { 260, 380 }, { 640, 470 }, { 560, 440 }, { 560, 440 }, { 480, 28 + 28 + 360 }, { 480, 360 } };
    *w = sz[id][0]; *h = sz[id][1];
}
void app_resize(int id, int cw, int ch) {
    wcw[id] = cw; wch[id] = ch;
    if (id == APP_TERM) {
        tcols = (cw - 16) / 8; if (tcols > TC) tcols = TC;
        trows = (ch - 12) / 16;
    } else if (id == APP_NOTES) {
        ncols = (cw - 16) / 8; nrows = (ch - 22 - 8) / 16;
    }
}
void app_setup(int id, int cw, int ch) {
    fs_init();
    app_resize(id, cw, ch);
    if (id == APP_TERM) {
        tl[0][0] = 0;
        t_puts("AuroraOS 1.0 - escreve 'help' para ver os comandos.\n");
        t_setprompt();
        t_puts(tprompt);
    } else if (id == APP_PAINT) {
        pcw = cw; pch = ch - PTOOL;
        pcv = (uint32_t *)hal_alloc((uint32_t)pcw * (uint32_t)pch * 4u);
        imgcap = pcw * pch; enccap = 6 + 2 * imgcap;
        imgbuf = (uint8_t *)hal_alloc((uint32_t)imgcap);
        encbuf = (uint8_t *)hal_alloc((uint32_t)enccap);
        p_clear();
        make_sample();
    } else if (id == APP_MEM) {
        mem_refresh();
    } else if (id == APP_SNAKE) {
        sn_reset();
    }
}
static void clear_session_state(void) {
    tn = 1; tl[0][0] = 0; tinlen = 0; tlast[0] = 0;
    nlen = 0; ncur = 0; ntop = 0; nmsg[0] = 0;
    if (pcv) p_clear();
    pfile[0] = 0; pmsg[0] = 0; mmsg[0] = 0; umsg[0] = 0; uinput = 0; usel = 0; u_wipe();
    fmsg[0] = 0; finput = 0; fclip = -1; fsel = 0; ftop = 0; pvid = -1;
}
void apps_session_start(const char *home) {
    clear_session_state();
    kstrcpy(homepath, home);
    int h = fs_find(home);
    tcwd = h >= 0 ? h : 0; fdir = tcwd;
    kstrcpy(nfile, kstrcmp(home, "/") ? home : "");
    kstrcpy(nfile + kstrlen(nfile), "/nota.txt");
    usel = users_cur() >= 0 ? users_cur() : 0;
    t_puts("Bem-vindo ao AuroraOS, "); t_puts(users_name(users_cur())); t_puts("! Escreve 'help' para ver os comandos.\n");
    t_setprompt(); t_puts(tprompt);
}
void apps_session_end(void) {
    clear_session_state();
    kstrcpy(homepath, "/"); tcwd = 0; fdir = 0;
    kstrcpy(nfile, "/nota.txt");
}
void apps_fs_changed(void) {
    { int h = fs_find(homepath); tcwd = h >= 0 ? h : 0; }
    fdir = tcwd; fsel = 0; ftop = 0; fclip = -1; finput = 0; pvid = -1; fclick_row = -1;
    fmsg[0] = 0;
    t_setprompt();
}
int app_tick(int id, uint32_t ms) { return id == APP_SNAKE ? snake_tick(ms) : 0; }
void app_draw(int id, int x, int y, int w, int h) {
    switch (id) {
    case APP_TERM: term_draw(x, y, w, h); break;
    case APP_NOTES: notes_draw(x, y, w, h); break;
    case APP_PAINT: paint_draw(x, y, w, h); break;
    case APP_CALC: calc_draw(x, y, w, h); break;
    case APP_FILES: files_draw(x, y, w, h); break;
    case APP_MEM: mem_draw(x, y, w, h); break;
    case APP_USERS: users_draw(x, y, w, h); break;
    case APP_SNAKE: snake_draw(x, y, w, h); break;
    case APP_ABOUT: about_draw(x, y, w, h); break;
    }
}
void app_key(int id, int k) {
    switch (id) {
    case APP_TERM: term_key(k); break;
    case APP_NOTES: notes_key(k); break;
    case APP_PAINT: paint_key(k); break;
    case APP_CALC: calc_key(k); break;
    case APP_FILES: files_key(k); break;
    case APP_MEM: mem_key(k); break;
    case APP_USERS: users_key(k); break;
    case APP_SNAKE: snake_key(k); break;
    }
}
void app_click(int id, int rx, int ry) {
    if (id == APP_PAINT) paint_click(rx, ry);
    else if (id == APP_FILES) files_click(rx, ry);
    else if (id == APP_MEM) mem_click(rx, ry);
    else if (id == APP_USERS) users_click(rx, ry);
    else if (id == APP_CALC) calc_click(rx, ry, wcw[id], wch[id]);
}
void app_drag(int id, int rx, int ry) { if (id == APP_PAINT) paint_drag(rx, ry); }
void app_up(int id) { if (id == APP_PAINT) plx = ply = -1; }
