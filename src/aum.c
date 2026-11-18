/* AuroraMemory (.aum): guarda/carrega o sistema de ficheiros num disco ATA.
 *
 * Disco (sectores de 512 bytes):
 *   sector 0 : cabecalho   "AUM1", versao(u32: 1 ou 2), payload(u32), crc32(u32), registos(u32),
 *                          ano(u16) mes dia hora min (u8), n.o de utilizadores (u8, v2)
 *   sector 1+: payload     registos em profundidade: [tipo 1=pasta 2=ficheiro][pai 255=raiz]
 *                          [tamanho do nome][nome Latin-1][tamanho u32][dados]
 *              v2: depois dos registos, o bloco de utilizadores:
 *                          [n u8] n x ([tam. nome u8][nome][admin u8][sal 8][hash SHA-256 32])
 * Todos os inteiros sao little-endian. O CRC e o CRC-32 padrao (igual ao zlib.crc32). */
#include "os.h"

static uint32_t crct[256];
static int crc_ready, saved_ver, has_disk;
static int auto_flag, synced, suspended, seen_ver, last_err;
static uint32_t change_ms, fail_until;

static void crc_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crct[i] = c;
    }
    crc_ready = 1;
}
static uint32_t crc_upd(uint32_t crc, const uint8_t *p, int n) {
    while (n--) crc = crct[(crc ^ *p++) & 255] ^ (crc >> 8);
    return crc;
}
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t get32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

/* ---------- escrita em fluxo ---------- */
typedef struct { int write, disk, err, fill, recs; uint32_t len, crc, lba; uint8_t st[512]; } Wr;
static void w_put(Wr *w, const void *p, int n) {
    const uint8_t *b = (const uint8_t *)p;
    w->crc = crc_upd(w->crc, b, n);
    w->len += (uint32_t)n;
    if (!w->write) return;
    while (n > 0) {
        int c = 512 - w->fill; if (c > n) c = n;
        for (int i = 0; i < c; i++) w->st[w->fill + i] = b[i];
        w->fill += c; b += c; n -= c;
        if (w->fill == 512) {
            if (hal_disk_write(w->disk, w->lba++, w->st, 1) < 0) w->err = 1;
            w->fill = 0;
        }
    }
}
static void put_cb(void *c, const void *p, int n) { w_put((Wr *)c, p, n); }
static void w_dir(Wr *w, int dir, int parent_rec) {
    int ids[64], n = fs_list(dir, ids, 64);
    for (int i = 0; i < n; i++) {
        int id = ids[i], isd = fs_isdir(id);
        const char *nm = fs_nm(id);
        uint8_t h[3] = { (uint8_t)(isd ? 1 : 2), (uint8_t)parent_rec, (uint8_t)kstrlen(nm) };
        uint8_t l4[4]; uint32_t len = isd ? 0 : (uint32_t)fs_size(id);
        put32(l4, len);
        w_put(w, h, 3); w_put(w, nm, h[2]); w_put(w, l4, 4);
        if (!isd) {
            uint8_t buf[512];
            for (uint32_t off = 0; off < len; ) {
                int c = (int)(len - off < 512 ? len - off : 512);
                fs_rd_at(id, (int)off, buf, c);
                w_put(w, buf, c);
                off += (uint32_t)c;
            }
        }
        int me = w->recs++;
        if (isd) w_dir(w, id, me);
    }
}

/* ---------- leitura em fluxo ---------- */
typedef struct { int disk; uint32_t lba, left; int pos; uint8_t st[512]; } Rd;
static int r_get(Rd *r, void *dst, int n) {
    uint8_t *d = (uint8_t *)dst;
    while (n > 0) {
        if (r->left == 0) return -1;
        if (r->pos == 512) { if (hal_disk_read(r->disk, r->lba++, r->st, 1) < 0) return -1; r->pos = 0; }
        int c = 512 - r->pos; if (c > n) c = n; if ((uint32_t)c > r->left) c = (int)r->left;
        for (int i = 0; i < c; i++) d[i] = r->st[r->pos + i];
        r->pos += c; r->left -= (uint32_t)c; n -= c; d += c;
    }
    return 0;
}

static int get_cb(void *c, void *d, int n) { return r_get((Rd *)c, d, n); }

/* ---------- classificacao dos discos ---------- */
void aum_probe(AumInfo *in) {
    uint8_t s[512];
    in->state = AUM_NODISK; in->disk = -1; in->foreign = 0; in->count = 0; in->users = 0; in->ver = 0; in->autom = 0; in->payload = 0; in->sectors = 0;
    in->y = in->mo = in->d = in->h = in->mi = 0;
    int blank = -1, nd = hal_disk_count();
    for (int d = 0; d < nd; d++) {
        uint32_t sec = hal_disk_sectors(d);
        if (hal_disk_read(d, 0, s, 1) < 0) continue;
        if (s[0] == 'A' && s[1] == 'U' && s[2] == 'M' && s[3] == '1') {
            in->disk = d; in->sectors = sec;
            uint32_t ver = get32(s + 4), pl = get32(s + 8);
            in->payload = pl; in->count = (int)get32(s + 16); in->ver = (int)ver;
            in->users = ver >= 2 ? s[26] : 0;
            in->autom = s[27] & 1; auto_flag = in->autom;
            in->y = s[20] | (s[21] << 8); in->mo = s[22]; in->d = s[23]; in->h = s[24]; in->mi = s[25];
            in->state = ((ver == 1 || ver == 2) && pl <= (sec - 1) * 512u) ? AUM_VALID : AUM_BAD;
            has_disk = 1;
            return;
        }
        int zero = 1;                                   /* "em branco": primeiros 8 KiB a zero */
        for (int i = 0; i < 512 && zero; i++) if (s[i]) zero = 0;
        for (uint32_t k = 1; k < 16 && k < sec && zero; k++) {
            if (hal_disk_read(d, k, s, 1) < 0) { zero = 0; break; }
            for (int i = 0; i < 512; i++) if (s[i]) { zero = 0; break; }
        }
        if (zero && sec <= 524288u) { if (blank < 0) { blank = d; in->sectors = sec; } }
        else in->foreign++;
    }
    if (blank >= 0) { in->state = AUM_BLANK; in->disk = blank; has_disk = 1; }
    else has_disk = 0;
    if (in->state == AUM_NODISK) auto_flag = 0;
}

int aum_dirty(void) { return has_disk && fs_ver() != saved_ver; }
void aum_mark_clean(void) { saved_ver = fs_ver(); }

static int aum_save_raw(int *need_kb) {
    if (!crc_ready) crc_init();
    AumInfo in; aum_probe(&in);
    if (in.state == AUM_NODISK) return -1;
    static Wr w;
    w.write = 0; w.err = 0; w.fill = 0; w.recs = 0; w.len = 0; w.crc = 0xFFFFFFFFu;
    w_dir(&w, 0, 255); users_emit(put_cb, &w);               /* passagem 1: tamanho */
    uint32_t need = (w.len + 511) / 512 + 1;
    if (need > in.sectors) { if (need_kb) *need_kb = (int)((need * 512 + 1023) / 1024); return -2; }
    w.write = 1; w.disk = in.disk; w.lba = 1; w.len = 0; w.crc = 0xFFFFFFFFu; w.recs = 0;
    w_dir(&w, 0, 255); users_emit(put_cb, &w);               /* passagem 2: escreve */
    if (w.fill > 0) {
        for (int i = w.fill; i < 512; i++) w.st[i] = 0;
        if (hal_disk_write(w.disk, w.lba++, w.st, 1) < 0) w.err = 1;
    }
    uint8_t h[512];
    for (int i = 0; i < 512; i++) h[i] = 0;
    h[0] = 'A'; h[1] = 'U'; h[2] = 'M'; h[3] = '1';
    put32(h + 4, 2); put32(h + 8, w.len); put32(h + 12, ~w.crc); put32(h + 16, (uint32_t)w.recs);
    int y, mo, d, hh, mm, ss;
    hal_date(&d, &mo, &y); hal_time(&hh, &mm, &ss);
    h[26] = (uint8_t)users_n(); h[27] = auto_flag ? 1 : 0;
    h[20] = (uint8_t)y; h[21] = (uint8_t)(y >> 8); h[22] = (uint8_t)mo; h[23] = (uint8_t)d; h[24] = (uint8_t)hh; h[25] = (uint8_t)mm;
    const char *tag = "AuroraMemory (.aum) - AuroraOS";
    for (int i = 0; tag[i]; i++) h[32 + i] = (uint8_t)tag[i];
    if (w.err || hal_disk_write(in.disk, 0, h, 1) < 0) return -3;   /* o cabecalho vai por ultimo */
    hal_disk_flush(in.disk);
    aum_mark_clean();
    synced = 1; last_err = 0;
    return 0;
}

static int aum_load_raw(void) {
    if (!crc_ready) crc_init();
    AumInfo in; aum_probe(&in);
    if (in.state != AUM_VALID) return -1;
    uint8_t s[512], buf[512];
    hal_disk_read(in.disk, 0, s, 1);
    uint32_t crc_want = get32(s + 12);
    int ver = (int)get32(s + 4);
    int count = (int)get32(s + 16);
    /* passagem 1: verificar o CRC */
    Rd r; r.disk = in.disk; r.lba = 1; r.left = in.payload; r.pos = 512;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t left = in.payload; left; ) {
        int c = (int)(left < 512 ? left : 512);
        if (r_get(&r, buf, c) < 0) return -2;
        crc = crc_upd(crc, buf, c); left -= (uint32_t)c;
    }
    if (~crc != crc_want || count < 0 || count > 63) return -2;
    /* passagem 2: reconstruir */
    r.lba = 1; r.left = in.payload; r.pos = 512;
    fs_reset();
    int map[64], ok = 1;
    for (int k = 0; k < count && ok; k++) {
        uint8_t h[3], l4[4]; char name[FS_NAME + 1];
        if (r_get(&r, h, 3) < 0 || h[2] >= FS_NAME || r_get(&r, name, h[2]) < 0 || r_get(&r, l4, 4) < 0) { ok = 0; break; }
        name[h[2]] = 0;
        int parent = h[1] == 255 ? 0 : (h[1] < k ? map[h[1]] : -1);
        int id = parent < 0 ? -1 : fs_mk(parent, name, h[0] == 1);
        if (id < 0) { ok = 0; break; }
        map[k] = id;
        uint32_t len = get32(l4);
        if (h[0] == 2) {
            while (len && ok) {
                int c = (int)(len < 512 ? len : 512);
                if (r_get(&r, buf, c) < 0 || fs_append(id, buf, c) < 0) ok = 0;
                len -= (uint32_t)c;
            }
        }
    }
    if (ok && ver >= 2 && users_parse(get_cb, &r) < 0) ok = 0;      /* v1 nao tem utilizadores: mantem os atuais */
    if (!ok) { fs_reset(); fs_defaults(); users_ensure_homes(); apps_fs_changed(); gui_session_check(); aum_mark_clean(); return -2; }
    users_ensure_homes();
    apps_fs_changed();
    gui_session_check();
    aum_mark_clean();
    synced = 1;
    return 0;
}

int aum_save(int *need_kb) { int old = fs_raw(1); int rc = aum_save_raw(need_kb); fs_raw(old); return rc; }
int aum_load(void) { int old = fs_raw(1); int rc = aum_load_raw(); fs_raw(old); return rc; }

/* ---------- memoria interna (guardar/carregar automatico) ---------- */
int aum_auto_flag(void) { return auto_flag; }
int aum_auto_active(void) { return auto_flag && !suspended && has_disk; }
int aum_synced(void) { return synced; }
void aum_suspend(void) { suspended = 1; }
int aum_led(void) {
    if (!has_disk) return 0;
    if (suspended || last_err) return 3;
    return fs_ver() == saved_ver ? 1 : 2;
}
int aum_set_auto(int on) {
    AumInfo in; aum_probe(&in);
    if (in.state == AUM_NODISK) return -1;
    auto_flag = on ? 1 : 0; suspended = 0; last_err = 0;
    if (in.state == AUM_VALID) {                         /* so o byte de flags do cabecalho */
        uint8_t s[512];
        if (hal_disk_read(in.disk, 0, s, 1) < 0) return -3;
        s[27] = (uint8_t)auto_flag;
        if (hal_disk_write(in.disk, 0, s, 1) < 0) return -3;
        hal_disk_flush(in.disk);
        return 0;
    }
    return aum_save(0) == 0 ? 0 : -3;                    /* disco novo: cria a memoria ja com o modo ligado */
}
void aum_autosave_tick(uint32_t ms) {
    if (!auto_flag || suspended || !has_disk || !synced) return;
    int v = fs_ver();
    if (v == saved_ver) { seen_ver = v; return; }
    if (v != seen_ver) { seen_ver = v; change_ms = ms; return; }   /* ainda a mudar: espera */
    if (ms - change_ms < 4000 || ms < fail_until) return;
    if (mem_do_save() != 0) { fail_until = ms + 30000; last_err = 1; }
}
