/* AuroraOS - kernel: HAL x86 (framebuffer VBE, teclado/rato PS/2, RTC, CPUID) */
#include "os.h"

extern uint32_t vbe_fb;                 /* preenchidas por boot.S */
extern uint16_t vbe_pitch, vbe_w, vbe_h;
extern uint8_t  vbe_bpp;
extern char __bss_start[], __bss_end[];

static inline void outb(uint16_t p, uint8_t v) { __asm__ volatile("outb %0,%1" :: "a"(v), "Nd"(p)); }
static inline void outw(uint16_t p, uint16_t v) { __asm__ volatile("outw %0,%1" :: "a"(v), "Nd"(p)); }
static inline uint8_t inb(uint16_t p) { uint8_t v; __asm__ volatile("inb %1,%0" : "=a"(v) : "Nd"(p)); return v; }

/* ---------- memoria: alocador linear a partir de 4 MiB ---------- */
static uint32_t heap = 0x400000;
void *hal_alloc(uint32_t n) {
    void *p = (void *)heap;
    heap += (n + 4095u) & ~4095u;
    uint32_t *q = (uint32_t *)p;
    for (uint32_t i = 0; i < (n + 3) / 4; i++) q[i] = 0;
    return p;
}

/* ---------- video ---------- */
void hal_present(int x, int y, int w, int h) {
    uint8_t *fb = (uint8_t *)vbe_fb;
    if (vbe_bpp == 32) {
        for (int j = 0; j < h; j++) {
            uint32_t *src = back + (y + j) * SW + x;
            uint32_t *dst = (uint32_t *)(fb + (uint32_t)(y + j) * vbe_pitch + (uint32_t)x * 4);
            int n = w;
            __asm__ volatile("rep movsl" : "+S"(src), "+D"(dst), "+c"(n) :: "memory");
        }
    } else {                             /* 24 bpp */
        for (int j = 0; j < h; j++) {
            uint32_t *src = back + (y + j) * SW + x;
            uint8_t *dst = fb + (uint32_t)(y + j) * vbe_pitch + (uint32_t)x * 3;
            for (int i = 0; i < w; i++) {
                uint32_t c = src[i];
                dst[0] = (uint8_t)c; dst[1] = (uint8_t)(c >> 8); dst[2] = (uint8_t)(c >> 16);
                dst += 3;
            }
        }
    }
}
const char *hal_video(void) {
    static char s[24];
    char t[12]; int n = 0;
    n += kitoa(SW, s + n); s[n++] = 'x';
    n += kitoa(SH, s + n); s[n++] = 'x';
    kitoa(vbe_bpp, t); kstrcpy(s + n, t);
    return s;
}

/* ---------- CPU ---------- */
const char *hal_cpu(void) {
    static char name[64];
    if (name[0]) return name;
    uint32_t a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0x80000000u));
    if (a >= 0x80000004u) {
        uint32_t *o = (uint32_t *)name;
        for (uint32_t leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
            __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(leaf));
            *o++ = a; *o++ = b; *o++ = c; *o++ = d;
        }
        name[48] = 0;
        char *p = name; while (*p == ' ') p++;
        if (p != name) kstrcpy(name, p);
    } else {
        __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0));
        *(uint32_t *)(name + 0) = b; *(uint32_t *)(name + 4) = d; *(uint32_t *)(name + 8) = c;
        name[12] = 0;
    }
    return name;
}

/* ---------- RTC ---------- */
static uint8_t cmos(uint8_t r) { outb(0x70, r); return inb(0x71); }
static void rtc_read(int *v) {          /* seg, min, hora, dia, mes, ano */
    int t;
    for (t = 0; t < 100000 && (cmos(0x0A) & 0x80); t++) ;
    uint8_t regs[6] = { 0x00, 0x02, 0x04, 0x07, 0x08, 0x09 };
    int bin = cmos(0x0B) & 4;
    for (int i = 0; i < 6; i++) {
        int x = cmos(regs[i]);
        if (!bin) x = (x & 15) + (x >> 4) * 10;
        v[i] = x;
    }
}
void hal_time(int *h, int *m, int *s) { int v[6]; rtc_read(v); *s = v[0]; *m = v[1]; *h = v[2]; }
void hal_date(int *d, int *mo, int *y) { int v[6]; rtc_read(v); *d = v[3]; *mo = v[4]; *y = 2000 + v[5]; }

/* ---------- temporizador: PIT canal 0 (le o contador, nao precisa de IRQ) ---------- */
uint32_t hal_ms(void) {
    static uint32_t ms, rem; static uint16_t last; static int init;
    outb(0x43, 0);                       /* latch do contador */
    uint16_t c = inb(0x40);
    c |= (uint16_t)(inb(0x40) << 8);
    if (!init) { init = 1; last = c; }
    rem += (uint16_t)(last - c);         /* o contador decrementa */
    last = c;
    ms += rem / 1193; rem %= 1193;
    return ms;
}

/* ---------- aleatorio (sal das palavras-passe): TSC + PIT + RTC ---------- */
uint32_t hal_random(void) {
    static uint32_t s = 0x9E3779B9u;
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    s ^= lo * 2654435761u; s ^= hi + (hal_ms() << 9) + ((uint32_t)cmos(0) << 20);
    s ^= s << 13; s ^= s >> 17; s ^= s << 5;
    return s;
}

/* ---------- energia ---------- */
static void halt_forever(void) { for (;;) __asm__ volatile("cli; hlt"); }
void hal_reboot(void) {
    for (int i = 0; i < 100000; i++) if (!(inb(0x64) & 2)) break;
    outb(0x64, 0xFE);                    /* pulso de reset no controlador de teclado */
    uint16_t idt[3] = { 0, 0, 0 };       /* fallback: triple fault */
    __asm__ volatile("lidt %0; int3" :: "m"(idt));
    halt_forever();
}
void hal_shutdown(void) {
    outw(0x604, 0x2000);                 /* QEMU (ACPI) */
    outw(0xB004, 0x2000);                /* Bochs / QEMU antigo */
    outw(0x4004, 0x3400);                /* VirtualBox */
    gx_fill(0, 0, SW, SH, 0);
    gx_text(SW / 2 - 17 * 8, SH / 2 - 8, "Pode desligar o computador.", RGB(200, 200, 200), 1);
    hal_present(0, 0, SW, SH);
    halt_forever();
}

/* ---------- teclado PS/2 (set 1), layout PT ---------- */
#define DEAD_ACUTE 0x1001
#define DEAD_GRAVE 0x1002
#define DEAD_TILDE 0x1003
#define DEAD_CIRC  0x1004
static const uint16_t KM[0x59][3] = {   /* normal, shift, altgr */
    [0x01] = {27, 27, 0},
    [0x02] = {'1','!',0}, [0x03] = {'2','"','@'}, [0x04] = {'3','#',0xA3}, [0x05] = {'4','$',0xA7},
    [0x06] = {'5','%',0}, [0x07] = {'6','&',0}, [0x08] = {'7','/','{'}, [0x09] = {'8','(','['},
    [0x0A] = {'9',')',']'}, [0x0B] = {'0','=','}'}, [0x0C] = {'\'','?',0}, [0x0D] = {0xAB,0xBB,0},
    [0x0E] = {'\b','\b',0}, [0x0F] = {9,9,0},
    [0x10] = {'q','Q',0}, [0x11] = {'w','W',0}, [0x12] = {'e','E',0}, [0x13] = {'r','R',0},
    [0x14] = {'t','T',0}, [0x15] = {'y','Y',0}, [0x16] = {'u','U',0}, [0x17] = {'i','I',0},
    [0x18] = {'o','O',0}, [0x19] = {'p','P',0}, [0x1A] = {'+','*',0},
    [0x1B] = {DEAD_ACUTE, DEAD_GRAVE, 0}, [0x1C] = {'\n','\n',0},
    [0x1E] = {'a','A',0}, [0x1F] = {'s','S',0}, [0x20] = {'d','D',0}, [0x21] = {'f','F',0},
    [0x22] = {'g','G',0}, [0x23] = {'h','H',0}, [0x24] = {'j','J',0}, [0x25] = {'k','K',0},
    [0x26] = {'l','L',0}, [0x27] = {0xE7,0xC7,0}, [0x28] = {0xBA,0xAA,0}, [0x29] = {'\\','|',0},
    [0x2B] = {DEAD_TILDE, DEAD_CIRC, 0},
    [0x2C] = {'z','Z',0}, [0x2D] = {'x','X',0}, [0x2E] = {'c','C',0}, [0x2F] = {'v','V',0},
    [0x30] = {'b','B',0}, [0x31] = {'n','N',0}, [0x32] = {'m','M',0}, [0x33] = {',',';',0},
    [0x34] = {'.',':',0}, [0x35] = {'-','_',0}, [0x39] = {' ',' ',0}, [0x56] = {'<','>',0},
};
static int sh_l, sh_r, altgr, caps, e0, dead, ctrl;

static int compose(int d, int c) {
    static const char vow[] = "aeiouAEIOU";
    int i;
    for (i = 0; vow[i] && vow[i] != c; i++) ;
    if (!vow[i]) {
        if (d == DEAD_TILDE && c == 'n') return 0xF1;
        if (d == DEAD_TILDE && c == 'N') return 0xD1;
        if (c == ' ') return d == DEAD_ACUTE ? 0xB4 : d == DEAD_GRAVE ? '`' : d == DEAD_TILDE ? '~' : '^';
        return c;
    }
    static const unsigned char acute[] = { 0xE1,0xE9,0xED,0xF3,0xFA, 0xC1,0xC9,0xCD,0xD3,0xDA };
    static const unsigned char grave[] = { 0xE0,0xE8,0xEC,0xF2,0xF9, 0xC0,0xC8,0xCC,0xD2,0xD9 };
    static const unsigned char circ[]  = { 0xE2,0xEA,0xEE,0xF4,0xFB, 0xC2,0xCA,0xCE,0xD4,0xDB };
    static const unsigned char tild[]  = { 0xE3, 'e', 'i', 0xF5, 'u', 0xC3, 'E', 'I', 0xD5, 'U' };
    switch (d) {
    case DEAD_ACUTE: return acute[i];
    case DEAD_GRAVE: return grave[i];
    case DEAD_CIRC:  return circ[i];
    default:         return tild[i];
    }
}

static void kbd_byte(uint8_t sc) {
    if (sc == 0xE0) { e0 = 1; return; }
    int rel = sc & 0x80, code = sc & 0x7F;
    int was_e0 = e0; e0 = 0;
    if (was_e0 && (code == 0x2A || code == 0x36)) return;   /* shifts "falsos" */
    if (code == 0x1D) { ctrl = !rel; return; }
    if (code == 0x2A) { sh_l = !rel; return; }
    if (code == 0x36) { sh_r = !rel; return; }
    if (code == 0x38 && was_e0) { altgr = !rel; return; }
    if (rel) return;
    if (was_e0) {
        switch (code) {
        case 0x48: gui_key(K_UP); break;
        case 0x50: gui_key(K_DOWN); break;
        case 0x4B: gui_key(K_LEFT); break;
        case 0x4D: gui_key(K_RIGHT); break;
        case 0x53: gui_key(K_DEL); break;
        case 0x47: gui_key(K_HOME); break;
        case 0x4F: gui_key(K_END); break;
        case 0x1C: gui_key('\n'); break;      /* Enter do teclado numerico */
        case 0x35: gui_key('/'); break;
        }
        return;
    }
    if (code == 0x3A) { caps = !caps; return; }
    if (code >= 0x59) return;
    int shift = sh_l || sh_r;
    int c = KM[code][0];
    if (!c) return;
    int letter = (c >= 'a' && c <= 'z') || c == 0xE7;
    int up = letter ? (shift ^ caps) : shift;
    if (altgr && KM[code][2]) c = KM[code][2];
    else if (up && KM[code][1]) c = KM[code][1];
    if (ctrl && (c | 0x20) >= 'a' && (c | 0x20) <= 'z') { gui_key((c | 0x20) - 'a' + 1); return; }
    if (c >= 0x1001 && c <= 0x1004) {
        if (dead) { gui_key(compose(dead, ' ')); dead = 0; }
        else dead = c;
        return;
    }
    if (dead) { c = compose(dead, c); dead = 0; }
    gui_key(c);
}

/* ---------- rato PS/2 ---------- */
static int m_idx; static uint8_t m_pkt[3];
static void mouse_byte(uint8_t d) {
    if (m_idx == 0 && !(d & 8)) return;           /* ressincronizar */
    m_pkt[m_idx++] = d;
    if (m_idx < 3) return;
    m_idx = 0;
    if (m_pkt[0] & 0xC0) return;                  /* overflow */
    int dx = (int8_t)m_pkt[1], dy = -(int8_t)m_pkt[2];
    gui_mouse(dx, dy, m_pkt[0] & 7);
}

static void kbc_wait_w(void) { for (int t = 0; t < 100000 && (inb(0x64) & 2); t++) ; }
static void kbc_wait_r(void) { for (int t = 0; t < 100000 && !(inb(0x64) & 1); t++) ; }
static void mouse_cmd(uint8_t v) {
    kbc_wait_w(); outb(0x64, 0xD4);
    kbc_wait_w(); outb(0x60, v);
    kbc_wait_r(); if (inb(0x64) & 1) inb(0x60);   /* ACK */
}
static void input_init(void) {
    while (inb(0x64) & 1) inb(0x60);
    kbc_wait_w(); outb(0x64, 0xA8);               /* activa porta auxiliar */
    kbc_wait_w(); outb(0x64, 0x20);
    kbc_wait_r(); uint8_t cfg = inb(0x60);
    cfg |= 0x43; cfg &= (uint8_t)~0x30;           /* IRQs + traducao set1; teclado e rato activos */
    kbc_wait_w(); outb(0x64, 0x60);
    kbc_wait_w(); outb(0x60, cfg);
    mouse_cmd(0xF6);                              /* defaults */
    mouse_cmd(0xF4);                              /* activa envio de dados */
    while (inb(0x64) & 1) inb(0x60);
}

/* ---------- discos ATA (PIO, LBA28) ---------- */
#define ATA_MAX 4
static struct { uint16_t base, ctl; int slave; uint32_t sectors; } ad[ATA_MAX];
static int nad = -1;

static void ata_delay(uint16_t ctl) { inb(ctl); inb(ctl); inb(ctl); inb(ctl); }
static int ata_wait(uint16_t base, int drq) {           /* espera BSY=0 (e DRQ=1 se pedido) */
    for (int t = 0; t < 500000; t++) {
        uint8_t s = inb(base + 7);
        if (s & 0x80) continue;
        if (s & 0x21) return -1;                        /* ERR ou DF */
        if (!drq || (s & 0x08)) return 0;
    }
    return -1;
}
static int ata_ident(uint16_t base, uint16_t ctl, int slave, uint32_t *sectors) {
    outb(base + 6, (uint8_t)(0xA0 | (slave << 4)));
    ata_delay(ctl);
    outb(base + 2, 0); outb(base + 3, 0); outb(base + 4, 0); outb(base + 5, 0);
    outb(base + 7, 0xEC);                               /* IDENTIFY */
    ata_delay(ctl);
    uint8_t s = inb(base + 7);
    if (s == 0 || s == 0xFF) return -1;                 /* sem unidade */
    int t;
    for (t = 0; t < 500000; t++) { s = inb(base + 7); if (!(s & 0x80)) break; }
    if (s & 0x80) return -1;
    if (inb(base + 4) || inb(base + 5)) return -1;      /* ATAPI/SATA: ignorar (ex.: o CD de arranque) */
    for (t = 0; t < 500000; t++) { s = inb(base + 7); if (s & 0x01) return -1; if (s & 0x08) break; }
    if (!(s & 0x08)) return -1;
    uint16_t id[256]; void *p = id; int n = 256;
    __asm__ volatile("rep insw" : "+D"(p), "+c"(n) : "d"(base) : "memory");
    *sectors = (uint32_t)id[60] | ((uint32_t)id[61] << 16);
    return *sectors ? 0 : -1;
}
int hal_disk_count(void) {
    if (nad >= 0) return nad;
    nad = 0;
    static const uint16_t bases[2] = { 0x1F0, 0x170 }, ctls[2] = { 0x3F6, 0x376 };
    for (int b = 0; b < 2; b++)
        for (int sl = 0; sl < 2; sl++) {
            uint32_t sec;
            if (nad < ATA_MAX && ata_ident(bases[b], ctls[b], sl, &sec) == 0) {
                ad[nad].base = bases[b]; ad[nad].ctl = ctls[b]; ad[nad].slave = sl; ad[nad].sectors = sec; nad++;
            }
        }
    return nad;
}
uint32_t hal_disk_sectors(int d) { return (d >= 0 && d < hal_disk_count()) ? ad[d].sectors : 0; }
static int ata_xfer(int d, uint32_t lba, void *buf, int write) {
    uint16_t base = ad[d].base, ctl = ad[d].ctl;
    if (lba >= ad[d].sectors || lba >= 0x10000000u) return -1;
    outb(base + 6, (uint8_t)(0xE0 | (ad[d].slave << 4) | ((lba >> 24) & 0x0F)));
    ata_delay(ctl);
    if (ata_wait(base, 0) < 0) return -1;
    outb(base + 2, 1);
    outb(base + 3, (uint8_t)lba); outb(base + 4, (uint8_t)(lba >> 8)); outb(base + 5, (uint8_t)(lba >> 16));
    outb(base + 7, write ? 0x30 : 0x20);
    ata_delay(ctl);
    if (ata_wait(base, 1) < 0) return -1;
    int n = 256;
    if (write) {
        const void *p = buf;
        __asm__ volatile("rep outsw" : "+S"(p), "+c"(n) : "d"(base) : "memory");
        ata_delay(ctl);
        if (ata_wait(base, 0) < 0) return -1;
    } else {
        void *p = buf;
        __asm__ volatile("rep insw" : "+D"(p), "+c"(n) : "d"(base) : "memory");
    }
    return 0;
}
int hal_disk_read(int d, uint32_t lba, void *buf, int n) {
    if (d < 0 || d >= hal_disk_count()) return -1;
    for (int i = 0; i < n; i++) if (ata_xfer(d, lba + (uint32_t)i, (uint8_t *)buf + i * 512, 0) < 0) return -1;
    return 0;
}
int hal_disk_write(int d, uint32_t lba, const void *buf, int n) {
    if (d < 0 || d >= hal_disk_count()) return -1;
    for (int i = 0; i < n; i++) if (ata_xfer(d, lba + (uint32_t)i, (void *)((const uint8_t *)buf + i * 512), 1) < 0) return -1;
    return 0;
}
void hal_disk_flush(int d) {
    if (d < 0 || d >= hal_disk_count()) return;
    outb(ad[d].base + 6, (uint8_t)(0xE0 | (ad[d].slave << 4)));
    outb(ad[d].base + 7, 0xE7);                         /* FLUSH CACHE */
    ata_wait(ad[d].base, 0);
}

/* ---------- entrada do kernel ---------- */
void kmain(void) {
    for (char *p = __bss_start; p < __bss_end; p++) *p = 0;
    __asm__ volatile("cli");
    gfx_init(vbe_w, vbe_h);
    gui_init();
    input_init();
    gui_flush();
    uint32_t spin = 0;
    for (;;) {
        uint8_t st;
        int guard = 0;
        while (((st = inb(0x64)) & 1) && guard++ < 256) {
            uint8_t d = inb(0x60);
            if (st & 0x20) mouse_byte(d); else kbd_byte(d);
        }
        if ((++spin & 0xFF) == 0) gui_tick();
        gui_flush();
        __asm__ volatile("pause");
    }
}
