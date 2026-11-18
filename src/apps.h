#ifndef APPS_H
#define APPS_H
#include "os.h"
enum { APP_TERM, APP_NOTES, APP_PAINT, APP_CALC, APP_FILES, APP_MEM, APP_USERS, APP_SNAKE, APP_ABOUT, NAPPS };
const char *app_title(int id);
void app_size(int id, int *w, int *h);             /* tamanho preferido da janela */
void app_setup(int id, int cw, int ch);            /* area de conteudo final */
void app_resize(int id, int cw, int ch);           /* maximizar/restaurar */
int  app_tick(int id, uint32_t ms);                /* 1 se precisa de redesenhar */
void notes_open_file(const char *path);
void paint_open_file(const char *path);
void *app_scratch(int n);
void apps_session_start(const char *home);        /* depois do login */
void apps_session_end(void);                       /* ao terminar sessao */
int  mem_do_save(void);                            /* guarda a RAM no .aum (mensagem na app Memoria) */
void mem_do_load(void);
void mem_set_msg(const char *m, int ok);
const char *mem_msg(void);                          /* buffer temporario (ou NULL) */
void app_draw(int id, int x, int y, int w, int h); /* desenha o conteudo */
void app_key(int id, int k);
void app_click(int id, int rx, int ry);
void app_drag(int id, int rx, int ry);
void app_up(int id);
#endif
