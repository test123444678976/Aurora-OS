/* Contas de utilizador: palavras-passe com sal + SHA-256 iterado (1000x). */
#include "os.h"
#define MAX_USERS 8
#define PW_MIN 4
typedef struct { char name[13]; int admin; uint8_t salt[8], hash[32]; } User;
static User U[MAX_USERS];
static int nu, cur = -1;

/* ---------- SHA-256 ---------- */
static uint32_t ror(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
static void sha_block(uint32_t *h, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) | ((uint32_t)p[4*i+2] << 8) | p[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ror(w[i-15], 7) ^ ror(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ror(w[i-2], 17) ^ ror(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = hh + (ror(e, 6) ^ ror(e, 11) ^ ror(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
        uint32_t t2 = (ror(a, 2) ^ ror(a, 13) ^ ror(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}
void sha256(const uint8_t *d, int len, uint8_t out[32]) {
    uint32_t h[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
    int i = 0;
    for (; i + 64 <= len; i += 64) sha_block(h, d + i);
    uint8_t blk[128];
    int r = len - i, total = r < 56 ? 64 : 128;
    for (int k = 0; k < 128; k++) blk[k] = 0;
    for (int k = 0; k < r; k++) blk[k] = d[i + k];
    blk[r] = 0x80;
    uint32_t bits = (uint32_t)len * 8u;
    blk[total - 4] = (uint8_t)(bits >> 24); blk[total - 3] = (uint8_t)(bits >> 16); blk[total - 2] = (uint8_t)(bits >> 8); blk[total - 1] = (uint8_t)bits;
    sha_block(h, blk);
    if (total == 128) sha_block(h, blk + 64);
    for (int k = 0; k < 8; k++) { out[4*k] = (uint8_t)(h[k] >> 24); out[4*k+1] = (uint8_t)(h[k] >> 16); out[4*k+2] = (uint8_t)(h[k] >> 8); out[4*k+3] = (uint8_t)h[k]; }
}
/* h0 = SHA256(sal||pw);  h(i) = SHA256(h(i-1)||sal||pw), i = 1..999 */
static void pwhash(const uint8_t *salt, const char *pw, uint8_t out[32]) {
    uint8_t m[8 + 32 + 40];
    int pl = kstrlen(pw); if (pl > 39) pl = 39;
    for (int i = 0; i < 8; i++) m[i] = salt[i];
    for (int i = 0; i < pl; i++) m[8 + i] = (uint8_t)pw[i];
    sha256(m, 8 + pl, out);
    for (int it = 1; it < 1000; it++) {
        for (int i = 0; i < 32; i++) m[i] = out[i];
        for (int i = 0; i < 8; i++) m[32 + i] = salt[i];
        for (int i = 0; i < pl; i++) m[40 + i] = (uint8_t)pw[i];
        sha256(m, 40 + pl, out);
    }
}

/* ---------- contas ---------- */
uint32_t users_color(int i) {
    static const uint32_t C[6] = { 0x2f6bff, 0x14a39a, 0x7b4dff, 0xe052c8, 0xf5842b, 0x46c35f };
    return C[(i < 0 ? 0 : i) % 6];
}
int users_n(void) { return nu; }
const char *users_name(int i) { return (i >= 0 && i < nu) ? U[i].name : "?"; }
int users_admin(int i) { return (i >= 0 && i < nu) ? U[i].admin : 0; }
int users_cur(void) { return cur; }
void users_set_cur(int i) { cur = i; }
void users_home(int i, char *out) { kstrcpy(out, "/home/"); kstrcpy(out + 6, users_name(i)); }
static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int same(const char *a, const char *b) { while (*a && lower(*a) == lower(*b)) { a++; b++; } return !*a && !*b; }
int users_find(const char *name) { for (int i = 0; i < nu; i++) if (same(U[i].name, name)) return i; return -1; }
int users_valid_name(const char *n) {
    int l = kstrlen(n);
    if (l < 1 || l > 12) return 0;
    for (int i = 0; i < l; i++) {
        char c = n[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}
static void fill_random(uint8_t *p, int n) { for (int i = 0; i < n; i++) p[i] = (uint8_t)(hal_random() >> 11); }

void users_ensure_homes(void) {
    int old = fs_raw(1);
    int home = fs_child(0, "home");
    if (home < 0) home = fs_mk(0, "home", 1);
    for (int i = 0; home >= 0 && i < nu; i++) {
        int h = fs_child(home, U[i].name);
        if (h < 0) h = fs_mk(home, U[i].name, 1);
        if (h < 0) continue;
        if (fs_child(h, "documentos") < 0) fs_mk(h, "documentos", 1);
        if (fs_child(h, "imagens") < 0) fs_mk(h, "imagens", 1);
    }
    fs_raw(old);
}
int users_add(const char *name, const char *pw, int admin) {
    if (!users_valid_name(name)) return -1;
    if (users_find(name) >= 0) return -2;
    if (nu >= MAX_USERS) return -3;
    if (kstrlen(pw) < PW_MIN) return -4;
    User *u = &U[nu];
    kstrcpy(u->name, name); u->admin = admin ? 1 : 0;
    fill_random(u->salt, 8);
    pwhash(u->salt, pw, u->hash);
    nu++;
    users_ensure_homes();
    fs_touch();
    return 0;
}
int users_check(int i, const char *pw) {
    if (i < 0 || i >= nu) return 0;
    uint8_t h[32]; int diff = 0;
    pwhash(U[i].salt, pw, h);
    for (int k = 0; k < 32; k++) diff |= h[k] ^ U[i].hash[k];
    return diff == 0;
}
int users_setpw(int i, const char *pw) {
    if (i < 0 || i >= nu) return -1;
    if (kstrlen(pw) < PW_MIN) return -4;
    fill_random(U[i].salt, 8);
    pwhash(U[i].salt, pw, U[i].hash);
    fs_touch();
    return 0;
}
static int n_admins(void) { int c = 0; for (int i = 0; i < nu; i++) c += U[i].admin; return c; }
int users_setadmin(int i, int a) {
    if (i < 0 || i >= nu) return -1;
    if (!a && U[i].admin && n_admins() <= 1) return -2;       /* tem de haver sempre um administrador */
    U[i].admin = a ? 1 : 0;
    fs_touch();
    return 0;
}
int users_del(int i) {
    if (i < 0 || i >= nu) return -1;
    if (U[i].admin && n_admins() <= 1) return -2;
    for (int k = i; k < nu - 1; k++) U[k] = U[k + 1];
    nu--;
    if (cur == i) cur = -1; else if (cur > i) cur--;
    fs_touch();
    return 0;
}

/* ---------- (de)serializacao para o .aum ---------- */
void users_emit(void (*put)(void *, const void *, int), void *ctx) {
    uint8_t n = (uint8_t)nu;
    put(ctx, &n, 1);
    for (int i = 0; i < nu; i++) {
        uint8_t nl = (uint8_t)kstrlen(U[i].name), a = (uint8_t)U[i].admin;
        put(ctx, &nl, 1); put(ctx, U[i].name, nl); put(ctx, &a, 1);
        put(ctx, U[i].salt, 8); put(ctx, U[i].hash, 32);
    }
}
int users_parse(int (*get)(void *, void *, int), void *ctx) {
    uint8_t n;
    static User t[MAX_USERS];
    if (get(ctx, &n, 1) < 0 || n > MAX_USERS) return -1;
    for (int i = 0; i < n; i++) {
        uint8_t nl, a;
        if (get(ctx, &nl, 1) < 0 || nl < 1 || nl > 12) return -1;
        if (get(ctx, t[i].name, nl) < 0) return -1;
        t[i].name[nl] = 0;
        if (!users_valid_name(t[i].name)) return -1;
        if (get(ctx, &a, 1) < 0 || get(ctx, t[i].salt, 8) < 0 || get(ctx, t[i].hash, 32) < 0) return -1;
        t[i].admin = a ? 1 : 0;
    }
    for (int i = 0; i < n; i++) U[i] = t[i];
    nu = n;
    return n;
}
