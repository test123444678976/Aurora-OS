/* Disco ATA simulado para os testes no host (um ficheiro em memoria). */
#include "../src/os.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t *dk; static uint32_t dsec;
void host_disk_blank(int mb) { free(dk); dk = calloc(1, (size_t)mb << 20); dsec = (uint32_t)(((size_t)mb << 20) / 512); }
void host_disk_load(const char *p) { FILE *f = fopen(p, "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f); free(dk); dk = calloc(1, n); fread(dk, 1, n, f); fclose(f); dsec = (uint32_t)(n / 512); }
void host_disk_dump(const char *p) { FILE *f = fopen(p, "wb"); fwrite(dk, 1, (size_t)dsec * 512, f); fclose(f); }
void host_disk_foreign(void) { host_disk_blank(4); dk[510] = 0x55; dk[511] = 0xAA; dk[0] = 0xEB; }
void host_disk_none(void) { free(dk); dk = 0; dsec = 0; }
int hal_disk_count(void) { return dk ? 1 : 0; }
uint32_t hal_disk_sectors(int d) { (void)d; return dsec; }
int hal_disk_read(int d, uint32_t lba, void *b, int n) { (void)d; if (!dk || lba + n > dsec) return -1; memcpy(b, dk + (size_t)lba * 512, (size_t)n * 512); return 0; }
int hal_disk_write(int d, uint32_t lba, const void *b, int n) { (void)d; if (!dk || lba + n > dsec) return -1; memcpy(dk + (size_t)lba * 512, b, (size_t)n * 512); return 0; }
void hal_disk_flush(int d) { (void)d; }

uint32_t hal_random(void) { return (uint32_t)rand(); }
