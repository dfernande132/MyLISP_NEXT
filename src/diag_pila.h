#ifndef DIAG_PILA_H
#define DIAG_PILA_H
#ifdef MYLISP_DIAG_PILA
void DiagPila(void);
void DiagIniciar(void);
void DiagCapturar(void);
void DiagInforme(void);
#define DIAG_PILA() DiagPila()
#else
#define DIAG_PILA()
#endif
#endif
