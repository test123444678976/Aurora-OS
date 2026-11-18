#include "../src/os.h"
#include <stdio.h>
static void gui_key_p(int k){ if(k>=32&&k<127) printf("%c",k); else printf("<%02x>",k); }
#define gui_key gui_key_p
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
static int sh_l, sh_r, altgr, caps, e0, dead;

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
    if (c >= 0x1001 && c <= 0x1004) {
        if (dead) { gui_key(compose(dead, ' ')); dead = 0; }
        else dead = c;
        return;
    }
    if (dead) { c = compose(dead, c); dead = 0; }
    gui_key(c);
}


#undef gui_key
static void K(int sc){ kbd_byte(sc); kbd_byte(sc|0x80); }
int main(void){
  K(0x1E);                              /* a */
  kbd_byte(0x2A);K(0x1E);kbd_byte(0xAA);/* A */
  K(0x2B);K(0x1E);                      /* ~ a -> ã (e3) */
  K(0x1B);K(0x12);                      /* ´ e -> é (e9) */
  kbd_byte(0x2A);K(0x2B);kbd_byte(0xAA);K(0x18); /* ^ o -> ô (f4) */
  K(0x27);                              /* ç (e7) */
  kbd_byte(0xE0);kbd_byte(0x38);K(0x03);kbd_byte(0xE0);kbd_byte(0xB8); /* AltGr+2 = @ */
  kbd_byte(0x2A);K(0x03);kbd_byte(0xAA);/* " */
  K(0x3A);K(0x1E);K(0x3A);K(0x1E);      /* caps: A a */
  K(0x39);K(0x56);kbd_byte(0xE0);K(0x4B);
  puts(""); return 0; }
