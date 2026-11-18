#include "apps.h"

#define TB_H    40
#define TITLE_H 28

typedef struct { int open, min, max, x, y, w, h, sx, sy, sw, sh; } Win;
static Win W[NAPPS];
static int Z[NAPPS], nz;               /* ordem de empilhamento (fundo -> topo) */
static int focus = -1;
static int mx, my, mbtn;
static int dragging = -1, dox, doy;    /* janela a ser arrastada */
static int grab = -1;                  /* app que recebeu o clique no conteudo */
static int menu_open;
static int need_redraw = 1, mouse_moved = 1;
static int last_hover = -2;
static int hh, mm, ss, dd, mo, yy, last_sec = -1;
static uint32_t *wall;
static int session;                   /* 0 = ecra de login, 1 = ambiente de trabalho */
static char sess_name[16];
static int last_click_id = -1;
static uint32_t last_click_ms, last_rtc_ms;

/* ---------- cores ---------- */
#define C_ACCENT   RGB(94, 230, 196)
#define C_ACCENT2  RGB(91, 108, 255)
#define C_TASKBAR  RGB(20, 24, 40)

/* ---------- papel de parede ---------- */
static void make_wall(void) {
    wall = (uint32_t *)hal_alloc((uint32_t)SW * (uint32_t)SH * 4u);
    for (int y = 0; y < SH; y++) {
        int t = y * 256 / SH;
        uint32_t base = t < 128 ? gx_mix(RGB(7, 10, 32), RGB(18, 44, 92), t * 2)
                                : gx_mix(RGB(18, 44, 92), RGB(30, 110, 118), (t - 128) * 2);
        for (int x = 0; x < SW; x++) wall[y * SW + x] = base;
    }
    /* "auroras": manchas suaves */
    struct { int cx, cy, r; uint32_t c; } b[] = {
        { SW * 70 / 100, SH * 30 / 100, SH * 55 / 100, RGB(94, 230, 196) },
        { SW * 35 / 100, SH * 55 / 100, SH * 50 / 100, RGB(91, 108, 255) },
        { SW * 90 / 100, SH * 85 / 100, SH * 45 / 100, RGB(190, 90, 255) },
        { SW * 15 / 100, SH * 15 / 100, SH * 35 / 100, RGB(60, 170, 255) },
    };
    for (int k = 0; k < 4; k++) {
        int r = b[k].r;
        long r2 = (long)r * r;
        int y0 = b[k].cy - r < 0 ? 0 : b[k].cy - r, y1 = b[k].cy + r > SH ? SH : b[k].cy + r;
        int x0 = b[k].cx - r < 0 ? 0 : b[k].cx - r, x1 = b[k].cx + r > SW ? SW : b[k].cx + r;
        for (int y = y0; y < y1; y++)
            for (int x = x0; x < x1; x++) {
                long dx = x - b[k].cx, dy = (y - b[k].cy) * 3 / 2;
                long d2 = dx * dx + dy * dy;
                if (d2 >= r2) continue;
                int a = (int)(256 - d2 * 256 / r2);   /* 0..256 */
                a = a * a >> 8;                       /* mais suave */
                wall[y * SW + x] = gx_mix(wall[y * SW + x], b[k].c, a * 70 / 256);
            }
    }
    /* estrelas */
    uint32_t s = 12345;
    for (int i = 0; i < SW * SH / 6000; i++) {
        s = s * 1103515245u + 12345u;
        int x = (s >> 8) % SW;
        s = s * 1103515245u + 12345u;
        int y = ((s >> 8) % SH) * 3 / 4;
        wall[y * SW + x] = gx_mix(wall[y * SW + x], RGB(255, 255, 255), 150 + (s & 63));
    }
}

/* ---------- gestao de janelas ---------- */
static void z_remove(int id) {
    int j = 0;
    for (int i = 0; i < nz; i++) if (Z[i] != id) Z[j++] = Z[i];
    nz = j;
}
static void z_top(int id) { z_remove(id); Z[nz++] = id; focus = id; need_redraw = 1; }
static void focus_next(void) {
    focus = -1;
    for (int i = nz - 1; i >= 0; i--) if (!W[Z[i]].min) { focus = Z[i]; break; }
}
static void win_open(int id) {
    if (!W[id].open) { W[id].open = 1; }
    W[id].min = 0;
    z_top(id);
}
static void win_close(int id) {
    W[id].open = 0; W[id].min = 0;
    z_remove(id);
    if (focus == id) focus_next();
    need_redraw = 1;
}
static void win_toggle_max(int id) {
    Win *w = &W[id];
    if (!w->max) {
        w->sx = w->x; w->sy = w->y; w->sw = w->w; w->sh = w->h;
        w->x = 0; w->y = 0; w->w = SW; w->h = SH - TB_H; w->max = 1;
    } else {
        w->x = w->sx; w->y = w->sy; w->w = w->sw; w->h = w->sh; w->max = 0;
    }
    app_resize(id, w->w, w->h - TITLE_H);
    need_redraw = 1;
}
static void win_minimize(int id) {
    W[id].min = 1;
    if (focus == id) focus_next();
    need_redraw = 1;
}
void gui_open(const char *n) {
    int id = -1;
    if (!kstrcmp(n, "terminal")) id = APP_TERM;
    else if (!kstrcmp(n, "notas") || !kstrcmp(n, "notes")) id = APP_NOTES;
    else if (!kstrcmp(n, "paint")) id = APP_PAINT;
    else if (!kstrcmp(n, "calc") || !kstrcmp(n, "calculadora")) id = APP_CALC;
    else if (!kstrcmp(n, "ficheiros") || !kstrcmp(n, "files") || !kstrcmp(n, "explorer")) id = APP_FILES;
    else if (!kstrcmp(n, "memoria") || !kstrcmp(n, "aum")) id = APP_MEM;
    else if (!kstrcmp(n, "utilizadores") || !kstrcmp(n, "users") || !kstrcmp(n, "contas")) id = APP_USERS;
    else if (!kstrcmp(n, "snake")) id = APP_SNAKE;
    else if (!kstrcmp(n, "sobre") || !kstrcmp(n, "about")) id = APP_ABOUT;
    if (id >= 0) win_open(id);
}

/* ---------- geometria ---------- */
#define N_ICONS NAPPS
static int canmax(int id) { return id <= APP_FILES; }
static void icon_rect(int i, int *x, int *y, int *w, int *h) {
    int per = (SH - TB_H - 20) / 76; if (per < 1) per = 1; if (per > N_ICONS) per = N_ICONS;
    int p = (SH - TB_H - 20) / per; if (p > 94) p = 94;
    *x = 10 + (i / per) * 104; *y = 14 + (i % per) * p; *w = 96; *h = p - 4;
}

static const char *MENU_L[] = { "Terminal", "Bloco de Notas", "Paint", "Calculadora", "Ficheiros", "Memória (.aum)", "Utilizadores", "Snake", "Sobre o AuroraOS", "Terminar sessão", "Reiniciar", "Desligar" };
#define MENU_N (NAPPS + 3)
#define MENU_HDR 36
static int menu_item_y(int i) { return 8 + MENU_HDR + i * 30 + (i >= NAPPS ? 10 : 0); }
static void menu_rect(int *x, int *y, int *w, int *h) {
    *w = 230; *h = menu_item_y(MENU_N - 1) + 30 + 8; *x = 6; *y = SH - TB_H - *h - 4;
}
static int tb_bw(void) { int w = (SW - 112 - 250) / NAPPS - 6; return w > 150 ? 150 : (w < 40 ? 40 : w); }
static void chip_rect(int *x, int *w) { *w = gx_textw(users_name(users_cur()), 1) + 40; *x = SW - 102 - *w; }
static int tb_btn_x(int id) {         /* posicao do botao da janela na barra */
    int x = 112;
    for (int i = 0; i < id; i++) if (W[i].open) x += tb_bw() + 6;
    return x;
}

/* ---------- desenho ---------- */
static void draw_icon_art(int i, int x, int y) {
    switch (i) {
    case 0:
        gx_rrect(x, y + 4, 44, 36, 5, RGB(60, 66, 96));
        gx_rrect(x + 2, y + 6, 40, 32, 4, RGB(14, 17, 24));
        gx_text(x + 6, y + 12, ">_", C_ACCENT, 2);
        break;
    case 1:
        gx_rrect(x + 6, y, 32, 44, 4, RGB(250, 250, 246));
        for (int k = 0; k < 5; k++) gx_fill(x + 11, y + 9 + k * 7, 22 - (k == 4 ? 8 : 0), 2, RGB(110, 130, 210));
        break;
    case 2:
        gx_rrect(x, y + 2, 44, 40, 14, RGB(214, 170, 120));
        gx_circle(x + 12, y + 16, 5, RGB(229, 72, 77));
        gx_circle(x + 24, y + 11, 5, RGB(247, 208, 44));
        gx_circle(x + 35, y + 18, 5, RGB(70, 195, 95));
        gx_circle(x + 13, y + 29, 5, RGB(47, 107, 255));
        gx_circle(x + 30, y + 33, 4, RGB(255, 255, 255));
        break;
    case 3:
        gx_rrect(x + 4, y, 36, 44, 5, RGB(52, 57, 82));
        gx_rrect(x + 8, y + 5, 28, 10, 2, RGB(94, 230, 196));
        for (int r = 0; r < 3; r++) for (int c = 0; c < 3; c++)
            gx_rrect(x + 8 + c * 10, y + 19 + r * 8, 8, 6, 1, r == 2 && c == 2 ? RGB(47, 107, 255) : RGB(190, 196, 220));
        break;
    case 4:
        gx_rrect(x + 2, y + 8, 40, 32, 5, RGB(236, 180, 60));
        gx_rrect(x + 2, y + 3, 18, 10, 3, RGB(214, 150, 40));
        gx_rrect(x + 2, y + 15, 40, 25, 5, RGB(250, 210, 100));
        gx_fill(x + 10, y + 24, 24, 2, RGB(214, 150, 40));
        gx_fill(x + 10, y + 30, 16, 2, RGB(214, 150, 40));
        break;
    case 5:
        for (int k = 0; k < 5; k++) { gx_fill(x + 8 + k * 7, y + 1, 3, 6, RGB(190, 196, 220)); gx_fill(x + 8 + k * 7, y + 37, 3, 6, RGB(190, 196, 220)); }
        gx_rrect(x + 2, y + 5, 40, 34, 5, RGB(94, 230, 196));
        gx_rrect(x + 7, y + 10, 30, 24, 3, RGB(34, 40, 90));
        gx_text(x + 10, y + 14, "AUM", RGB(255, 255, 255), 1);
        gx_fill(x + 11, y + 29, 22, 2, RGB(94, 230, 196));
        break;
    case 6:
        gx_circle(x + 22, y + 22, 21, RGB(91, 108, 255));
        gx_circle(x + 22, y + 16, 7, RGB(255, 255, 255));
        gx_rrect(x + 9, y + 26, 26, 14, 7, RGB(255, 255, 255));
        break;
    case 7: {
        static const int sx[6] = { 4, 4, 12, 20, 28, 28 }, sy[6] = { 8, 24, 24, 24, 24, 8 };
        gx_rrect(x, y + 2, 44, 40, 6, RGB(24, 30, 46));
        for (int k = 0; k < 6; k++) gx_rrect(x + 3 + sx[k] * 1, y + 6 + sy[k], 8, 8, 2, k == 5 ? RGB(150, 255, 220) : RGB(70, 200, 120));
        gx_circle(x + 38, y + 12, 3, RGB(240, 80, 90));
        break;
    }
    case 8:
        gx_circle(x + 22, y + 22, 21, RGB(47, 107, 255));
        gx_circle(x + 22, y + 22, 17, RGB(60, 130, 255));
        gx_text(x + 14, y + 6, "i", RGB(255, 255, 255), 2);
        gx_fill(x + 20, y + 22, 4, 4, RGB(255, 255, 255));
        break;
    }
}

/* ---------- dialogos modais ---------- */
enum { DA_CLOSE = 1, DA_LOAD, DA_PWR_SAVE, DA_PWR_NOSAVE, DA_AUTO_ON, DA_AUTO_FORCE };
static struct { int active, nb, n; char title[48]; char line[5][84]; char bl[3][24]; int ba[3]; } dlg;
static int pw_reboot;
static void dlg_set(const char *t) { dlg.active = 1; dlg.n = 0; dlg.nb = 0; kstrcpy(dlg.title, t); need_redraw = 1; }
static void dlg_line(const char *s) { if (dlg.n < 5) kstrcpy(dlg.line[dlg.n++], s); }
static void dlg_btn(const char *l, int a) { if (dlg.nb < 3) { kstrcpy(dlg.bl[dlg.nb], l); dlg.ba[dlg.nb++] = a; } }
static void dlg_geom(int *x, int *y, int *w, int *h) {
    *w = SW - 20 < 580 ? SW - 20 : 580; *h = 56 + dlg.n * 20 + 16 + 44;
    *x = (SW - *w) / 2; *y = (SH - *h) / 2;
}
static void dlg_btn_rect(int i, int *x, int *y, int *w, int *h) {
    int gx, gy, gw, gh; dlg_geom(&gx, &gy, &gw, &gh);
    int bw = 150, total = dlg.nb * bw + (dlg.nb - 1) * 12;
    *x = gx + (gw - total) / 2 + i * (bw + 12); *y = gy + gh - 44; *w = bw; *h = 30;
}
static int dlg_hover(void) {
    for (int i = 0; i < dlg.nb; i++) {
        int x, y, w, h; dlg_btn_rect(i, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) return 200 + i;
    }
    return -1;
}
static void do_power(void) { if (pw_reboot) hal_reboot(); else hal_shutdown(); }
static void dlg_action(int a) {
    dlg.active = 0; need_redraw = 1;
    if (a == DA_LOAD) mem_do_load();
    else if (a == DA_AUTO_ON || a == DA_AUTO_FORCE) mem_do_auto(1);
    else if (a == DA_PWR_NOSAVE) do_power();
    else if (a == DA_PWR_SAVE) {
        if (mem_do_save() == 0) do_power();
        else {
            dlg_set("Não foi possível guardar");
            dlg_line(mem_msg());
            dlg_btn("Sair sem guardar", DA_PWR_NOSAVE); dlg_btn("Cancelar", DA_CLOSE);
        }
    }
}
void gui_power(int reboot) {
    pw_reboot = reboot;
    if (aum_dirty() && aum_auto_active()) {                /* memoria interna: guarda e sai */
        if (mem_do_save() == 0) { do_power(); return; }
        dlg_set("Não foi possível guardar");
        dlg_line(mem_msg());
        dlg_btn("Sair sem guardar", DA_PWR_NOSAVE); dlg_btn("Cancelar", DA_CLOSE);
        return;
    }
    if (aum_dirty()) {
        dlg_set("Alterações por guardar");
        dlg_line("Há alterações na memória que ainda não foram guardadas");
        dlg_line(reboot ? "no .aum. Queres guardar antes de reiniciar?" : "no .aum. Queres guardar antes de desligar?");
        dlg_btn("Guardar e sair", DA_PWR_SAVE); dlg_btn("Sair sem guardar", DA_PWR_NOSAVE); dlg_btn("Cancelar", DA_CLOSE);
    } else do_power();
}
void gui_request_auto(int on) {
    if (session && !users_admin(users_cur())) { mem_set_msg("Apenas administradores mudam a memória interna.", 0); need_redraw = 1; return; }
    AumInfo in; aum_probe(&in);
    if (on && in.state == AUM_VALID && !aum_synced()) {    /* o disco tem dados que nao foram carregados */
        dlg_set("Ligar a memória interna?");
        dlg_line("O disco tem dados que não carregaste. Ao ligar, a memória");
        dlg_line("interna vai substituí-los pelo conteúdo atual da RAM.");
        dlg_btn("Substituir e ligar", DA_AUTO_FORCE); dlg_btn("Cancelar", DA_CLOSE);
        return;
    }
    mem_do_auto(on);
    need_redraw = 1;
}
void gui_request_load(void) {
    if (session && !users_admin(users_cur())) { mem_do_load(); need_redraw = 1; return; }   /* mostra a recusa */
    if (aum_dirty()) {
        dlg_set("Substituir a memória atual?");
        dlg_line("A RAM tem alterações por guardar. Carregar o .aum");
        dlg_line("vai substituí-las pelo conteúdo do disco.");
        dlg_btn("Carregar", DA_LOAD); dlg_btn("Cancelar", DA_CLOSE);
    } else { mem_do_load(); need_redraw = 1; }
}
static void boot_dialog(void) {
    AumInfo ai; aum_probe(&ai);
    aum_mark_clean();
    if (ai.state == AUM_VALID && ai.autom) {                 /* memoria interna: carrega sozinha */
        if (aum_load() == 0) { mem_set_msg("Memória interna carregada no arranque.", 1); return; }
        aum_suspend();
        dlg_set("Memória interna");
        dlg_line("A memória interna está ilegível (CRC inválido). O");
        dlg_line("auto-guardar ficou desligado para não a sobrescrever.");
        dlg_btn("Ok", DA_CLOSE);
        return;
    }
    dlg_set("Memória (.aum)");
    if (ai.state == AUM_VALID) {
        char b[84], t[12];
        dlg_line("Foi encontrada memória guardada (.aum) no disco ligado.");
        kpad2(b, ai.d); b[2] = '/'; kpad2(b + 3, ai.mo); b[5] = '/'; kitoa(ai.y, b + 6);
        kstrcpy(b + kstrlen(b), "  "); kpad2(t, ai.h); t[2] = ':'; kpad2(t + 3, ai.mi); t[5] = 0; kstrcpy(b + kstrlen(b), t);
        kstrcpy(b + kstrlen(b), "  -  "); kitoa(ai.count, t); kstrcpy(b + kstrlen(b), t); kstrcpy(b + kstrlen(b), " itens, ");
        kitoa((int)((ai.payload + 1023) / 1024), t); kstrcpy(b + kstrlen(b), t); kstrcpy(b + kstrlen(b), " KB");
        dlg_line(b);
        dlg_line("Carregar agora substitui o que está na RAM.");
        dlg_btn("Carregar", DA_LOAD); dlg_btn("Ignorar", DA_CLOSE);
    } else if (ai.state == AUM_BLANK) {
        dlg_line("Foi detetado um disco novo (vazio).");
        dlg_line("Queres usá-lo como memória interna? Tudo o que fizeres");
        dlg_line("fica guardado sozinho e volta a carregar no arranque.");
        dlg_btn("Usar como interna", DA_AUTO_ON); dlg_btn("Agora não", DA_CLOSE);
    } else if (ai.state == AUM_BAD) {
        dlg_line("Foi encontrado um .aum, mas o cabeçalho é inválido.");
        dlg_line("Se guardares a memória, ele será reescrito.");
        dlg_btn("Ok", DA_CLOSE);
    } else {
        dlg_line(ai.foreign ? "Só há discos que não são .aum (ignorados por segurança)." : "Nenhuma memória ligada: o que fizeres perde-se ao desligar.");
        dlg_line("Usa ./run.sh, ou: -drive file=memoria.aum,format=raw,if=ide,index=1");
        dlg_btn("Continuar", DA_CLOSE);
    }
}
static void draw_dialog(int hv) {
    gx_shade(0, 0, SW, SH, RGB(0, 0, 10), 150);
    int x, y, w, h; dlg_geom(&x, &y, &w, &h);
    gx_shade(x + 6, y + 8, w, h, RGB(0, 0, 0), 120);
    gx_rrect(x - 1, y - 1, w + 2, h + 2, 11, RGB(94, 108, 190));
    gx_rrect(x, y, w, h, 10, RGB(26, 30, 50));
    gx_textb(x + 20, y + 16, dlg.title, C_ACCENT);
    gx_fill(x + 16, y + 40, w - 32, 1, RGB(60, 66, 110));
    for (int i = 0; i < dlg.n; i++) gx_text(x + 20, y + 54 + i * 20, dlg.line[i], RGB(226, 230, 246), 1);
    for (int i = 0; i < dlg.nb; i++) {
        int bx, by, bw, bh; dlg_btn_rect(i, &bx, &by, &bw, &bh);
        uint32_t bg = i == 0 ? RGB(47, 107, 255) : (i == dlg.nb - 1 ? RGB(80, 86, 120) : RGB(190, 100, 50));
        if (hv == 200 + i) bg = gx_mix(bg, RGB(255, 255, 255), 50);
        gx_rrect(bx, by, bw, bh, 6, bg);
        gx_text(bx + (bw - gx_textw(dlg.bl[i], 1)) / 2, by + 7, dlg.bl[i], RGB(255, 255, 255), 1);
    }
}

static int hover_of(void) {
    if (dlg.active) return dlg_hover();
    if (!session) return login_hover(mx, my);
    for (int i = 0; i < N_ICONS; i++) {
        int x, y, w, h; icon_rect(i, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) return i;
    }
    for (int k = nz - 1; k >= 0; k--) {          /* botoes da barra de titulo */
        Win *w = &W[Z[k]];
        if (w->min) continue;
        if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h) continue;
        int rx = mx - w->x, ry = my - w->y;
        if (ry >= 4 && ry < 24 && rx >= w->w - 30 && rx < w->w - 6) return 100 + Z[k] * 3;
        if (ry >= 4 && ry < 24 && rx >= w->w - 58 && rx < w->w - 34) return 101 + Z[k] * 3;
        if (canmax(Z[k]) && ry >= 4 && ry < 24 && rx >= w->w - 86 && rx < w->w - 62) return 102 + Z[k] * 3;
        break;
    }
    if (menu_open) {
        int x, y, w, h; menu_rect(&x, &y, &w, &h);
        for (int i = 0; i < MENU_N; i++) {
            int iy = y + menu_item_y(i);
            if (mx >= x + 6 && mx < x + w - 6 && my >= iy && my < iy + 28) return 10 + i;
        }
    }
    if (my >= SH - TB_H) {
        if (mx >= 6 && mx < 102) return 20;
        { int cx, cw; chip_rect(&cx, &cw); if (mx >= cx && mx < cx + cw) return 40; }
        for (int i = 0; i < NAPPS; i++) if (W[i].open) {
            int bx = tb_btn_x(i);
            if (mx >= bx && mx < bx + tb_bw()) return 30 + i;
        }
    }
    return -1;
}

static void draw_icons(int hv) {
    static const char *lbl[N_ICONS] = { "Terminal", "Notas", "Paint", "Calculadora", "Ficheiros", "Memória", "Utilizadores", "Snake", "Sobre" };
    for (int i = 0; i < N_ICONS; i++) {
        int x, y, w, h; icon_rect(i, &x, &y, &w, &h);
        if (hv == i) { gx_rrect(x, y, w, h, 8, RGB(255, 255, 255)); gx_shade(x, y, w, h, RGB(20, 30, 70), 190); }
        draw_icon_art(i, x + (w - 44) / 2, y + 8);
        int tw = gx_textw(lbl[i], 1);
        gx_text(x + (w - tw) / 2 + 1, y + 61, lbl[i], RGB(0, 0, 0), 1);
        gx_text(x + (w - tw) / 2, y + 60, lbl[i], RGB(240, 244, 255), 1);
    }
}

static void draw_window(int id) {
    Win *w = &W[id];
    int act = id == focus;
    gx_shade(w->x + 5, w->y + 7, w->w, w->h, RGB(0, 0, 10), act ? 110 : 60);
    gx_fill(w->x - 1, w->y - 1, w->w + 2, w->h + 2, act ? RGB(94, 108, 190) : RGB(52, 56, 78));
    gx_vgrad(w->x, w->y, w->w, TITLE_H, act ? RGB(58, 76, 150) : RGB(46, 50, 72), act ? RGB(36, 48, 104) : RGB(34, 37, 56));
    gx_textb(w->x + 10, w->y + 6, app_title(id), act ? RGB(255, 255, 255) : RGB(170, 176, 200));
    /* botoes */
    int cx = w->x + w->w - 30, nx = w->x + w->w - 58, by = w->y + 4;
    int hc = mx >= cx && mx < cx + 24 && my >= by && my < by + 20;
    int hn = mx >= nx && mx < nx + 24 && my >= by && my < by + 20;
    gx_rrect(nx, by, 24, 20, 4, hn ? RGB(110, 120, 170) : RGB(70, 80, 124));
    gx_fill(nx + 7, by + 13, 10, 2, RGB(255, 255, 255));
    if (canmax(id)) {
        int xx = w->x + w->w - 86;
        int hm = mx >= xx && mx < xx + 24 && my >= by && my < by + 20;
        gx_rrect(xx, by, 24, 20, 4, hm ? RGB(110, 120, 170) : RGB(70, 80, 124));
        gx_rect(xx + 7, by + 5, 10, 10, RGB(255, 255, 255));
        gx_fill(xx + 7, by + 5, 10, 2, RGB(255, 255, 255));
    }
    gx_rrect(cx, by, 24, 20, 4, hc ? RGB(240, 80, 86) : RGB(190, 58, 72));
    for (int k = 0; k < 8; k++) { gx_px(cx + 8 + k, by + 5 + k, RGB(255, 255, 255)); gx_px(cx + 9 + k, by + 5 + k, RGB(255, 255, 255));
                                  gx_px(cx + 15 - k, by + 5 + k, RGB(255, 255, 255)); gx_px(cx + 16 - k, by + 5 + k, RGB(255, 255, 255)); }
    app_draw(id, w->x, w->y + TITLE_H, w->w, w->h - TITLE_H);
}

static void draw_taskbar(int hv) {
    int y = SH - TB_H;
    gx_fill(0, y, SW, TB_H, C_TASKBAR);
    gx_fill(0, y, SW, 1, RGB(60, 70, 120));
    /* botao iniciar */
    gx_rrect(6, y + 5, 96, 30, 6, hv == 20 || menu_open ? RGB(110, 126, 255) : C_ACCENT2);
    gx_fill(16, y + 13, 6, 6, RGB(255, 255, 255)); gx_fill(24, y + 13, 6, 6, C_ACCENT);
    gx_fill(16, y + 21, 6, 6, C_ACCENT);           gx_fill(24, y + 21, 6, 6, RGB(255, 255, 255));
    gx_textb(38, y + 12, "Aurora", RGB(255, 255, 255));
    /* janelas abertas */
    for (int i = 0; i < NAPPS; i++) if (W[i].open) {
        int bx = tb_btn_x(i);
        uint32_t bg = (i == focus && !W[i].min) ? RGB(58, 68, 120) : (hv == 30 + i ? RGB(44, 50, 84) : RGB(32, 37, 60));
        gx_rrect(bx, y + 5, tb_bw(), 30, 6, bg);
        if (i == focus && !W[i].min) gx_fill(bx + 10, y + 33, tb_bw() - 20, 2, C_ACCENT);
        char tt[24]; int mc = (tb_bw() - 16) / 8, k = 0;
        while (app_title(i)[k] && k < mc && k < 22) { tt[k] = app_title(i)[k]; k++; }
        tt[k] = 0;
        gx_text(bx + 8, y + 12, tt, W[i].min ? RGB(130, 138, 170) : RGB(232, 236, 250), 1);
    }
    /* relogio */
    char t[12], d[12];
    kpad2(t, hh); t[2] = ':'; kpad2(t + 3, mm); t[5] = ':'; kpad2(t + 6, ss); t[8] = 0;
    kpad2(d, dd); d[2] = '/'; kpad2(d + 3, mo); d[5] = '/'; kitoa(yy, d + 6);
    gx_text(SW - 8 - 64 - 8, y + 2, t, RGB(240, 244, 255), 1);
    gx_text(SW - 8 - 80 - 4, y + 20, d, RGB(150, 160, 200), 1);
    { /* LED da memoria: verde = guardada, laranja = por guardar, vermelho = erro */
        int led = aum_led(), cx, cw; chip_rect(&cx, &cw);
        if (led) {
            gx_circle(cx - 12, y + 20, 5, led == 1 ? RGB(60, 200, 130) : led == 2 ? RGB(247, 170, 60) : RGB(240, 80, 90));
            if (aum_auto_active()) gx_rect(cx - 18, y + 14, 12, 12, RGB(60, 70, 120));
        }
    }
    { /* utilizador em sessao */
        int cx, cw; chip_rect(&cx, &cw);
        gx_rrect(cx, y + 5, cw, 30, 6, hv == 40 ? RGB(58, 68, 120) : RGB(32, 37, 60));
        gx_circle(cx + 16, y + 20, 10, users_color(users_cur()));
        char c0 = users_name(users_cur())[0]; char ini[2] = { (c0 >= 'a' && c0 <= 'z') ? (char)(c0 - 32) : c0, 0 };
        gx_text(cx + 12, y + 12, ini, RGB(255, 255, 255), 1);
        gx_text(cx + 32, y + 12, users_name(users_cur()), RGB(232, 236, 250), 1);
    }
}

static void draw_menu(int hv) {
    int x, y, w, h; menu_rect(&x, &y, &w, &h);
    gx_shade(x + 4, y + 6, w, h, RGB(0, 0, 10), 110);
    gx_rrect(x - 1, y - 1, w + 2, h + 2, 9, RGB(70, 80, 140));
    gx_rrect(x, y, w, h, 8, RGB(26, 30, 50));
    gx_circle(x + 24, y + 22, 12, users_color(users_cur()));
    { char c0 = users_name(users_cur())[0]; char ini[2] = { (c0 >= 'a' && c0 <= 'z') ? (char)(c0 - 32) : c0, 0 }; gx_text(x + 20, y + 14, ini, RGB(255, 255, 255), 1); }
    gx_textb(x + 44, y + 8, users_name(users_cur()), RGB(255, 255, 255));
    gx_text(x + 44, y + 22, users_admin(users_cur()) ? "administrador" : "utilizador", RGB(130, 142, 200), 1);
    gx_fill(x + 12, y + MENU_HDR, w - 24, 1, RGB(60, 66, 110));
    for (int i = 0; i < MENU_N; i++) {
        int iy = y + menu_item_y(i);
        if (i == NAPPS) gx_fill(x + 12, iy - 6, w - 24, 1, RGB(70, 76, 110));
        if (hv == 10 + i) gx_rrect(x + 6, iy, w - 12, 28, 6, i >= NAPPS + 1 ? RGB(150, 50, 64) : RGB(60, 74, 150));
        gx_text(x + 20, iy + 6, MENU_L[i], RGB(236, 240, 255), 1);
    }
}

static void draw_all(int hv) {
    for (int i = 0; i < SW * SH; i++) back[i] = wall[i];
    if (!session) login_draw(hv);
    else {
        draw_icons(hv);
        for (int i = 0; i < nz; i++) if (!W[Z[i]].min) draw_window(Z[i]);
        draw_taskbar(hv);
        if (menu_open) draw_menu(hv);
    }
    if (dlg.active) draw_dialog(hv);
}

/* ---------- cursor (com salvaguarda do fundo) ---------- */
#define CUR_W 12
#define CUR_H 19
static const char *CUR[CUR_H] = {
    "X           ", "XX          ", "X.X         ", "X..X        ", "X...X       ", "X....X      ",
    "X.....X     ", "X......X    ", "X.......X   ", "X........X  ", "X.....XXXXX ", "X..X..X     ",
    "X.X X..X    ", "XX  X..X    ", "X    X..X   ", "     X..X   ", "      X..X  ", "      X..X  ",
    "       XX   " };
static uint32_t under[CUR_W * CUR_H];
static int cur_drawn, cux, cuy;

static void present_clip(int x, int y, int w, int h) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > SW) w = SW - x;
    if (y + h > SH) h = SH - y;
    if (w > 0 && h > 0) hal_present(x, y, w, h);
}
static void cursor_hide(void) {
    if (!cur_drawn) return;
    for (int j = 0; j < CUR_H; j++) for (int i = 0; i < CUR_W; i++) {
        int x = cux + i, y = cuy + j;
        if ((unsigned)x < (unsigned)SW && (unsigned)y < (unsigned)SH) back[y * SW + x] = under[j * CUR_W + i];
    }
    present_clip(cux, cuy, CUR_W, CUR_H);
    cur_drawn = 0;
}
static void cursor_show(void) {
    cux = mx; cuy = my;
    for (int j = 0; j < CUR_H; j++) for (int i = 0; i < CUR_W; i++) {
        int x = cux + i, y = cuy + j;
        if ((unsigned)x >= (unsigned)SW || (unsigned)y >= (unsigned)SH) continue;
        under[j * CUR_W + i] = back[y * SW + x];
        char c = CUR[j][i];
        if (c == 'X') back[y * SW + x] = RGB(0, 0, 0);
        else if (c == '.') back[y * SW + x] = RGB(255, 255, 255);
    }
    cur_drawn = 1;
    present_clip(cux, cuy, CUR_W, CUR_H);
}

/* ---------- eventos ---------- */
static void do_menu_action(int i) {
    menu_open = 0; need_redraw = 1;
    if (i < NAPPS) win_open(i);
    else if (i == NAPPS) gui_logout();
    else if (i == NAPPS + 1) gui_power(1);
    else if (i == NAPPS + 2) gui_power(0);
}

static void on_press(void) {
    /* menu iniciar */
    if (menu_open) {
        int x, y, w, h; menu_rect(&x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) {
            int hv = hover_of();
            if (hv >= 10 && hv < 10 + MENU_N) do_menu_action(hv - 10);
            return;
        }
        menu_open = 0; need_redraw = 1;
        if (mx >= 6 && mx < 102 && my >= SH - TB_H) return;
    }
    /* barra de tarefas */
    if (my >= SH - TB_H) {
        if (mx >= 6 && mx < 102) { menu_open = 1; need_redraw = 1; return; }
        { int cx, cw; chip_rect(&cx, &cw); if (mx >= cx && mx < cx + cw) { win_open(APP_USERS); return; } }
        for (int i = 0; i < NAPPS; i++) if (W[i].open) {
            int bx = tb_btn_x(i);
            if (mx >= bx && mx < bx + tb_bw()) {
                if (i == focus && !W[i].min) win_minimize(i); else win_open(i);
                return;
            }
        }
        return;
    }
    /* janelas */
    for (int i = nz - 1; i >= 0; i--) {
        int id = Z[i]; Win *w = &W[id];
        if (W[id].min) continue;
        if (mx < w->x || mx >= w->x + w->w || my < w->y || my >= w->y + w->h) continue;
        z_top(id);
        int rx = mx - w->x, ry = my - w->y;
        if (ry < TITLE_H) {
            if (rx >= w->w - 30 && rx < w->w - 6 && ry >= 4 && ry < 24) { win_close(id); return; }
            if (rx >= w->w - 58 && rx < w->w - 34 && ry >= 4 && ry < 24) { win_minimize(id); return; }
            if (canmax(id) && rx >= w->w - 86 && rx < w->w - 62 && ry >= 4 && ry < 24) { win_toggle_max(id); return; }
            uint32_t t = hal_ms();
            if (canmax(id) && id == last_click_id && t - last_click_ms < 400) { last_click_id = -1; win_toggle_max(id); return; }
            last_click_id = id; last_click_ms = t;
            if (w->max) return;
            dragging = id; dox = mx - w->x; doy = my - w->y;
        } else {
            grab = id;
            app_click(id, rx, ry - TITLE_H);
        }
        return;
    }
    /* icones */
    for (int i = 0; i < N_ICONS; i++) {
        int x, y, w, h; icon_rect(i, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) { win_open(i); return; }
    }
}

void gui_mouse(int dx, int dy, int btn) {
    mx += dx; my += dy;
    if (mx < 0) mx = 0; if (mx >= SW) mx = SW - 1;
    if (my < 0) my = 0; if (my >= SH) my = SH - 1;
    if (dx || dy) mouse_moved = 1;
    int down = btn & 1, was = mbtn & 1;
    mbtn = btn;
    if (dlg.active) {                                   /* dialogo modal */
        if (down && !was) { int hv = dlg_hover(); if (hv >= 200) dlg_action(dlg.ba[hv - 200]); }
        return;
    }
    if (!session) { if (down && !was) login_click(mx, my); return; }
    if (down && !was) on_press();
    else if (down && dragging >= 0) {
        Win *w = &W[dragging];
        w->x = mx - dox; w->y = my - doy;
        if (w->y < 0) w->y = 0;
        if (w->y > SH - TB_H - TITLE_H) w->y = SH - TB_H - TITLE_H;
        if (w->x < -w->w + 80) w->x = -w->w + 80;
        if (w->x > SW - 80) w->x = SW - 80;
        need_redraw = 1;
    } else if (down && grab >= 0 && (dx || dy)) {
        app_drag(grab, mx - W[grab].x, my - W[grab].y - TITLE_H);
        need_redraw = 1;
    }
    if (!down && was) {
        if (grab >= 0) app_up(grab);
        dragging = -1; grab = -1;
    }
}

void gui_key(int k) {
    if (dlg.active) {
        if (k == '\n') dlg_action(dlg.ba[0]);
        else if (k == 27) dlg_action(dlg.ba[dlg.nb - 1]);
        return;
    }
    if (!session) { login_key(k); need_redraw = 1; return; }
    if (k == 27 && menu_open) { menu_open = 0; need_redraw = 1; return; }
    if (focus >= 0) { app_key(focus, k); need_redraw = 1; }
}

void gui_tick(void) {
    uint32_t ms = hal_ms();
    aum_autosave_tick(ms);
    for (int i = 0; i < NAPPS; i++)
        if (session && W[i].open && !W[i].min && app_tick(i, ms)) need_redraw = 1;
    if (ms - last_rtc_ms >= 200 || last_sec < 0) {
        last_rtc_ms = ms;
        int h, m, s;
        hal_time(&h, &m, &s);
        if (s != last_sec) {
            last_sec = s; hh = h; mm = m; ss = s;
            hal_date(&dd, &mo, &yy);
            need_redraw = 1;
        }
    }
}

void gui_flush(void) {
    int hv = hover_of();
    /* cursor sobre botoes da janela: tambem muda o aspeto */
    if (hv != last_hover) { last_hover = hv; need_redraw = 1; }
    if (need_redraw) {
        cur_drawn = 0;
        draw_all(hv);
        cursor_show();
        hal_present(0, 0, SW, SH);
        need_redraw = 0; mouse_moved = 0;
    } else if (mouse_moved) {
        cursor_hide();
        cursor_show();
        mouse_moved = 0;
    }
}

/* ---------- sessoes ---------- */
static void close_all(void) {
    for (int i = 0; i < NAPPS; i++) { W[i].open = 0; W[i].min = 0; }
    nz = 0; focus = -1; menu_open = 0; dragging = -1; grab = -1;
}
void gui_login_ok(int uid) {
    char home[40];
    users_set_cur(uid);
    kstrcpy(sess_name, users_name(uid));
    users_ensure_homes();
    fs_set_user(users_name(uid), users_admin(uid));
    close_all();
    session = 1;
    users_home(uid, home);
    apps_session_start(home);
    win_open(APP_TERM);
    need_redraw = 1;
}
void gui_logout(void) {
    if (!session) return;
    session = 0;
    users_set_cur(-1);
    fs_set_nouser();
    sess_name[0] = 0;
    close_all();
    apps_session_end();
    login_reset();
    need_redraw = 1;
}
void gui_session_check(void) {                       /* depois de carregar um .aum ou mudar contas */
    if (!session) { login_reset(); need_redraw = 1; return; }
    int i = users_find(sess_name);
    if (i < 0) { gui_logout(); return; }
    users_set_cur(i);
    fs_set_user(users_name(i), users_admin(i));
    need_redraw = 1;
}

/* ---------- init ---------- */
void gui_init(void) {
    make_wall();
    mx = SW / 2; my = SH / 2;
    for (int i = 0; i < NAPPS; i++) {
        int w, h;
        app_size(i, &w, &h);
        if (w > SW - 130) w = SW - 130;
        if (h > SH - TB_H - 20) h = SH - TB_H - 20;
        W[i].w = w; W[i].h = h;
        W[i].x = 130 + (i % 5) * 34 + ((N_ICONS * 76 > SH - TB_H - 20) ? 100 : 0); W[i].y = 24 + (i % 5) * 30;
        if (W[i].x + w > SW) W[i].x = SW - w - 10;
        if (W[i].y + h > SH - TB_H) W[i].y = SH - TB_H - h - 4;
        app_setup(i, w, h - TITLE_H);
    }
    gui_tick();
    session = 0;
    login_reset();
    boot_dialog();
}
