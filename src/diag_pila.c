#include "diag_pila.h"
#ifdef MYLISP_DIAG_PILA
#include <stdio.h>
#include "colors.h"
extern unsigned char DiagBase;
unsigned int DiagInicio;
unsigned int DiagMin;
unsigned int DiagMarca;

/* Capturar ANTES de printf: el informe no se cuenta a si mismo.
   main vuelve a armar el patron despues de mostrar el resultado. */
void DiagInforme(void) {
    DiagCapturar();
    INK(COLOR_INFO);
    printf("MEM SP=%04X libre=%u marca=%u\n",
           DiagMin, DiagMin < (unsigned int)&DiagBase ? 0 :
                    DiagMin - (unsigned int)&DiagBase,
           DiagMarca - (unsigned int)&DiagBase);
    INK(COLOR_ENTRADA);
}
#endif
