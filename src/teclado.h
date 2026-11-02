#ifndef TECLADO_H
#define TECLADO_H
void TecladoInit(void);
void EsperarTecla(void);
/* 1 linea aceptada; 2 cancelada por Esc. max = caracteres sin terminador. */
int LeerTeclado(char *destino, int max);
#endif
