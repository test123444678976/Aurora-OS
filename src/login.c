/* Ecra de login: escolha de utilizador + palavra-passe; primeira utilizacao cria o administrador. */
#include "os.h"

static int lsel, lfield, lfails;
static char lpw[32], lname[16], lpw2[32], lerr[72];
static uint32_t lblock;

static void wipe(void) { for (int i = 0; i < 32; i++) lpw[i] = lpw2[i] = 0; }
void login_reset(void) {
    lsel = 0; lfield = 0; lfails = 0; lblock = 0;
    wipe(); lname[0] = 0; lerr[0] = 0;
}
static int setup_mode(void) { return users_n() == 0; }
static void seterr(const char *m) { int i = 0; while (m[i] && i < 70) { lerr[i] = m[i]; i++; } lerr[i] = 0; }

/* ---------- geometria ---------- */
static int cell(void) { int n = users_n(); return n > 0 && n * 90 > SW - 60 ? (SW - 60) / n : 90; }
static void card(int *x, int *y, int *w, int *h) {
    int n = users_n();
    *w = n * cell() + 60; if (*w < 460) *w = 460; if (*w > SW - 10) *w = SW - 10;
    *h = setup_mode() ? 350 : 290;
    *x = (SW - *w) / 2; *y = SH * 30 / 100;
    if (*y + *h > SH - 56) *y = SH - 56 - *h;
    if (*y < 8) *y = 8;
}
static void field_rect(int f, int *x, int *y, int *w, int *h) {         /* f: 0..2 (setup) ; 0 = palavra-passe (login) */
    int cx, cy, cw, ch; card(&cx, &cy, &cw, &ch);
    *w = cw - 80; *h = 30; *x = cx + 40;
    *y = setup_mode() ? cy + 112 + f * 58 : cy + 156;
}
static void btn_rect(int *x, int *y, int *w, int *h) {
    int cx, cy, cw, ch; card(&cx, &cy, &cw, &ch);
    *w = 140; *h = 34; *x = cx + (cw - 140) / 2; *y = cy + ch - 56;
}
static void power_rect(int i, int *x, int *y, int *w, int *h) { *w = 110; *h = 30; *x = SW - 16 - 110 - (1 - i) * 118; *y = SH - 46; }
static int inside(int mx, int my, int x, int y, int w, int h) { return mx >= x && mx < x + w && my >= y && my < y + h; }

int login_hover(int mx, int my) {
    int x, y, w, h;
    for (int i = 0; i < 2; i++) { power_rect(i, &x, &y, &w, &h); if (inside(mx, my, x, y, w, h)) return 321 + i; }
    btn_rect(&x, &y, &w, &h);
    if (inside(mx, my, x, y, w, h)) return setup_mode() ? 340 : 320;
    if (setup_mode()) {
        for (int f = 0; f < 3; f++) { field_rect(f, &x, &y, &w, &h); if (inside(mx, my, x, y, w, h)) return 330 + f; }
    } else {
        int cx, cy, cw, ch, n = users_n(), c = cell(); card(&cx, &cy, &cw, &ch);
        int sx = cx + (cw - n * c) / 2;
        for (int i = 0; i < n; i++) if (inside(mx, my, sx + i * c, cy + 20, c, 100)) return 300 + i;
        field_rect(0, &x, &y, &w, &h);
        if (inside(mx, my, x, y, w, h)) return 330;
    }
    return -1;
}

/* ---------- accoes ---------- */
static void do_login(void) {
    uint32_t t = hal_ms();
    if (t < lblock) { char b[40], n[8]; kitoa((int)((lblock - t + 999) / 1000), n); kstrcpy(b, "Aguarda "); kstrcpy(b + 8, n); kstrcpy(b + kstrlen(b), " s antes de tentar de novo."); seterr(b); return; }
    if (users_check(lsel, lpw)) { wipe(); lfails = 0; lerr[0] = 0; gui_login_ok(lsel); return; }
    lfails++; wipe();
    if (lfails >= 3) { int s = (lfails - 2) * 3; if (s > 15) s = 15; lblock = hal_ms() + (uint32_t)s * 1000u; seterr("Palavra-passe errada. Espera uns segundos."); }
    else seterr("Palavra-passe errada.");
}
static void do_setup(void) {
    int rc;
    if (!users_valid_name(lname)) seterr("Nome inválido (1-12 letras, números, - ou _).");
    else if (kstrlen(lpw) < 4) seterr("A palavra-passe precisa de pelo menos 4 caracteres.");
    else if (kstrcmp(lpw, lpw2)) seterr("As palavras-passe não coincidem.");
    else if ((rc = users_add(lname, lpw, 1)) != 0) seterr("Não foi possível criar a conta.");
    else { int id = users_find(lname); wipe(); lerr[0] = 0; gui_login_ok(id); }
}
void login_click(int mx, int my) {
    int hv = login_hover(mx, my);
    if (hv >= 300 && hv < 320) { lsel = hv - 300; wipe(); lerr[0] = 0; }
    else if (hv == 320) do_login();
    else if (hv == 340) do_setup();
    else if (hv >= 330 && hv < 333) lfield = hv - 330;
    else if (hv == 321) gui_power(1);
    else if (hv == 322) gui_power(0);
}
static char *field_buf(int f) { return f == 0 ? (setup_mode() ? lname : lpw) : f == 1 ? lpw : lpw2; }
void login_key(int k) {
    int n = users_n();
    if (setup_mode()) {
        char *b = field_buf(lfield); int l = kstrlen(b);
        if (k == 9 || k == K_DOWN) lfield = (lfield + 1) % 3;
        else if (k == K_UP) lfield = (lfield + 2) % 3;
        else if (k == '\n') { if (lfield < 2) lfield++; else do_setup(); }
        else if (k == '\b') { if (l > 0) b[l - 1] = 0; }
        else if (k >= 32 && k < 256 && k != 127 && l < (lfield == 0 ? 12 : 30) && (lfield != 0 || k > 32)) { b[l] = (char)k; b[l + 1] = 0; }
        return;
    }
    int l = kstrlen(lpw);
    if (k == K_LEFT || (k == 9 && n > 1)) { lsel = (lsel + (k == K_LEFT ? n - 1 : 1)) % n; wipe(); lerr[0] = 0; }
    else if (k == K_RIGHT) { lsel = (lsel + 1) % n; wipe(); lerr[0] = 0; }
    else if (k == '\n') do_login();
    else if (k == 27) { wipe(); lerr[0] = 0; }
    else if (k == '\b') { if (l > 0) lpw[l - 1] = 0; }
    else if (k >= 32 && k < 256 && k != 127 && l < 30) { lpw[l] = (char)k; lpw[l + 1] = 0; }
}

/* ---------- desenho ---------- */
static void draw_field(int f, const char *label, const char *buf, int masked, int active, int hot) {
    int x, y, w, h; field_rect(f, &x, &y, &w, &h);
    if (label) gx_text(x, y - 18, label, RGB(160, 170, 215), 1);
    gx_rrect(x - 1, y - 1, w + 2, h + 2, 7, active ? RGB(94, 230, 196) : (hot ? RGB(110, 120, 190) : RGB(60, 66, 110)));
    gx_rrect(x, y, w, h, 6, RGB(12, 15, 28));
    int tx = x + 12;
    if (masked) for (int i = 0; buf[i]; i++) { gx_circle(tx + 4, y + h / 2, 4, RGB(235, 238, 250)); tx += 14; }
    else tx = gx_text(tx, y + 7, buf, RGB(235, 238, 250), 1);
    if (active) gx_fill(tx, y + 6, 2, h - 12, RGB(94, 230, 196));
}
void login_draw(int hv) {
    gx_shade(0, 0, SW, SH, RGB(0, 0, 16), 120);
    int hh, mm, ss, dd, mo, yy;
    hal_time(&hh, &mm, &ss); hal_date(&dd, &mo, &yy);
    char t[8], d[16];
    kpad2(t, hh); t[2] = ':'; kpad2(t + 3, mm); t[5] = 0;
    kpad2(d, dd); d[2] = '/'; kpad2(d + 3, mo); d[5] = '/'; kitoa(yy, d + 6);
    int sc = SH < 600 ? 4 : 6;
    gx_text((SW - gx_textw(t, sc)) / 2, SH * 6 / 100, t, RGB(255, 255, 255), sc);
    gx_text((SW - gx_textw(d, 2)) / 2, SH * 6 / 100 + 16 * sc + 8, d, RGB(190, 200, 240), 2);

    int cx, cy, cw, ch; card(&cx, &cy, &cw, &ch);
    gx_shade(cx + 6, cy + 8, cw, ch, RGB(0, 0, 0), 110);
    gx_rrect(cx - 1, cy - 1, cw + 2, ch + 2, 15, RGB(80, 92, 170));
    gx_rrect(cx, cy, cw, ch, 14, RGB(20, 24, 44));
    int bx, by, bw, bh; btn_rect(&bx, &by, &bw, &bh);
    if (setup_mode()) {
        gx_text(cx + (cw - gx_textw("Primeira utilização", 2)) / 2, cy + 20, "Primeira utilização", RGB(94, 230, 196), 2);
        const char *s = "Cria a conta de administrador do sistema.";
        gx_text(cx + (cw - gx_textw(s, 1)) / 2, cy + 58, s, RGB(180, 190, 230), 1);
        draw_field(0, "Nome de utilizador", lname, 0, lfield == 0, hv == 330);
        draw_field(1, "Palavra-passe", lpw, 1, lfield == 1, hv == 331);
        draw_field(2, "Repetir palavra-passe", lpw2, 1, lfield == 2, hv == 332);
        gx_rrect(bx, by, bw, bh, 8, hv == 340 ? RGB(110, 140, 255) : RGB(47, 107, 255));
        gx_text(bx + (bw - gx_textw("Criar conta", 1)) / 2, by + 9, "Criar conta", RGB(255, 255, 255), 1);
    } else {
        int n = users_n(), c = cell(), sx = cx + (cw - n * c) / 2;
        for (int i = 0; i < n; i++) {
            int ax = sx + i * c + c / 2, ay = cy + 20 + 34, sel = i == lsel;
            if (sel) { gx_circle(ax, ay, 33, RGB(94, 230, 196)); gx_circle(ax, ay, 30, RGB(20, 24, 44)); }
            else if (hv == 300 + i) { gx_circle(ax, ay, 31, RGB(110, 120, 190)); gx_circle(ax, ay, 29, RGB(20, 24, 44)); }
            gx_circle(ax, ay, 27, users_color(i));
            char c0 = users_name(i)[0]; char ini[2] = { (c0 >= 'a' && c0 <= 'z') ? (char)(c0 - 32) : c0, 0 };
            gx_text(ax - 8, ay - 16, ini, RGB(255, 255, 255), 2);
            char nm[16]; int maxc = (c - 6) / 8; if (maxc > 12) maxc = 12;
            int k = 0; while (users_name(i)[k] && k < maxc) { nm[k] = users_name(i)[k]; k++; } nm[k] = 0;
            gx_text(ax - gx_textw(nm, 1) / 2, ay + 38, nm, sel ? RGB(255, 255, 255) : RGB(150, 160, 205), 1);
        }
        const char *role = users_admin(lsel) ? "administrador" : "utilizador";
        gx_text(cx + (cw - gx_textw(role, 1)) / 2, cy + 20 + 84 + 18, role, RGB(120, 132, 190), 1);
        draw_field(0, 0, lpw, 1, 1, hv == 330);
        gx_rrect(bx, by, bw, bh, 8, hv == 320 ? RGB(110, 140, 255) : RGB(47, 107, 255));
        gx_text(bx + (bw - gx_textw("Entrar", 1)) / 2, by + 9, "Entrar", RGB(255, 255, 255), 1);
    }
    if (lerr[0]) gx_text(cx + (cw - gx_textw(lerr, 1)) / 2, by - 24, lerr, RGB(255, 120, 130), 1);
    gx_text(16, SH - 38, "AuroraOS 1.0", RGB(190, 200, 240), 1);
    gx_text(16, SH - 20, setup_mode() ? "Tab muda de campo, Enter avança" : "Setas escolhem o utilizador, Enter entra", RGB(130, 140, 190), 1);
    static const char *pl[2] = { "Reiniciar", "Desligar" };
    for (int i = 0; i < 2; i++) {
        int x, y, w, h; power_rect(i, &x, &y, &w, &h);
        gx_rrect(x, y, w, h, 6, hv == 321 + i ? RGB(150, 60, 76) : RGB(60, 40, 64));
        gx_text(x + (w - gx_textw(pl[i], 1)) / 2, y + 7, pl[i], RGB(255, 255, 255), 1);
    }
}
