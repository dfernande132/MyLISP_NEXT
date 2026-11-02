#include <arch/z80.h>
#include <input.h>
#include "control.h"
int EjecucionCancelada;
int NuevoPendiente;
int PermitirNew;
int ConsultarInterrupcion(void) {
    /* CAPS SHIFT (fila FE) + SPACE (fila 7F). Sin eco ni paginacion. */
    if (in_Inkey() == 27 ||
        ((z80_inp(0xfefe) & 1) == 0 && (z80_inp(0x7ffe) & 1) == 0))
        EjecucionCancelada = 1;
    return EjecucionCancelada;
}
