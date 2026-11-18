#include "os.h"
#include "font.h"

uint32_t *back;
int SW, SH;

void gfx_init(int w, int h) {
    SW = w; SH = h;
    back = (uint32_t *)hal_alloc((uint32_t)w * (uint32_t)h * 4u);
}

uint32_t gx_mix(uint32_t d, uint32_t s, int a) {
    uint32_t r = ((((d >> 16) & 255) * (256 - a)) + (((s >> 16) & 255) * a)) >> 8;
    uint32_t g = ((((d >> 8) & 255) * (256 - a)) + (((s >> 8) & 255) * a)) >> 8;
    uint32_t b = (((d & 255) * (256 - a)) + ((s & 255) * a)) >> 8;
    return (r << 16) | (g << 8) | b;
}

void gx_px(int x, int y, uint32_t c) {
    if ((unsigned)x < (unsigned)SW && (unsigned)y < (unsigned)SH) back[y * SW + x] = c;
}

static int clip(int *x, int *y, int *w, int *h) {
    int x1 = *x + *w, y1 = *y + *h;
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    if (x1 > SW) x1 = SW;
    if (y1 > SH) y1 = SH;
    *w = x1 - *x; *h = y1 - *y;
    return *w > 0 && *h > 0;
}

void gx_fill(int x, int y, int w, int h, uint32_t c) {
    if (!clip(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *p = back + (y + j) * SW + x;
        for (int i = 0; i < w; i++) p[i] = c;
    }
}

void gx_rect(int x, int y, int w, int h, uint32_t c) {
    gx_fill(x, y, w, 1, c);
    gx_fill(x, y + h - 1, w, 1, c);
    gx_fill(x, y, 1, h, c);
    gx_fill(x + w - 1, y, 1, h, c);
}

void gx_shade(int x, int y, int w, int h, uint32_t c, int a) {
    if (!clip(&x, &y, &w, &h)) return;
    for (int j = 0; j < h; j++) {
        uint32_t *p = back + (y + j) * SW + x;
        for (int i = 0; i < w; i++) p[i] = gx_mix(p[i], c, a);
    }
}

void gx_vgrad(int x, int y, int w, int h, uint32_t c1, uint32_t c2) {
    for (int j = 0; j < h; j++)
        gx_fill(x, y + j, w, 1, gx_mix(c1, c2, h > 1 ? (j * 256) / (h - 1) : 0));
}

static int isqrt(int n) {
    int r = 0;
    while ((r + 1) * (r + 1) <= n) r++;
    return r;
}

void gx_rrect(int x, int y, int w, int h, int r, uint32_t c) {
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    for (int j = 0; j < h; j++) {
        int jj = j < r ? j : (j >= h - r ? h - 1 - j : -1);
        int inset = 0;
        if (jj >= 0) {
            int t = 2 * r - 2 * jj - 1;
            int s4 = isqrt(16 * r * r - 4 * t * t);
            inset = (4 * r - s4 + 2) / 4;
        }
        gx_fill(x + inset, y + j, w - 2 * inset, 1, c);
    }
}

void gx_circle(int cx, int cy, int r, uint32_t c) {
    for (int j = -r; j <= r; j++) {
        int d = isqrt(r * r - j * j);
        gx_fill(cx - d, cy + j, 2 * d + 1, 1, c);
    }
}

void gx_line(int x0, int y0, int x1, int y1, uint32_t c) {
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gx_px(x0, y0, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void gx_char(int x, int y, int ch, uint32_t c, int scale) {
    const uint8_t *g = font8x16[ch & 255];
    for (int row = 0; row < 16; row++) {
        uint8_t b = g[row];
        if (!b) continue;
        for (int col = 0; col < 8; col++) {
            if (b & (0x80 >> col)) {
                if (scale == 1) gx_px(x + col, y + row, c);
                else gx_fill(x + col * scale, y + row * scale, scale, scale, c);
            }
        }
    }
}

int gx_text(int x, int y, const char *s, uint32_t c, int scale) {
    for (; *s; s++) { gx_char(x, y, (unsigned char)*s, c, scale); x += 8 * scale; }
    return x;
}

void gx_textb(int x, int y, const char *s, uint32_t c) {
    gx_text(x, y, s, c, 1);
    gx_text(x + 1, y, s, c, 1);
}

int gx_textw(const char *s, int scale) { return kstrlen(s) * 8 * scale; }
