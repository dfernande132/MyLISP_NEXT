#include <stdio.h>
#include "gc.h"
#include "eval.h"
#include "pila_tareas.h"
#include "colors.h"

/* Las referencias temporales ya se han descartado. No llamar desde NewCell:
   los constructores tienen referencias vivas solo en variables C. */
void RecuperarRecursos(void) {
    int causa = HeapError;
    if (!causa) return;
    INK(COLOR_ERROR);
    switch (causa) {
        case RECURSO_SIMBOLOS: puts("ERR: symbol table exhausted"); break;
        case RECURSO_CADENAS: puts("ERR: string table exhausted"); break;
        case RECURSO_TAREAS: puts("ERR: evaluation task stack exhausted"); break;
        default: puts("ERR: heap cells exhausted"); break;
    }
    /* Eval retira sus tareas al salir; el REPL es el punto de recuperacion. */
    if (PilaTareasTope()) return;
    HeapError = 0;
    ErrorFlag = 0;
    EvalError = 0;
    CollectGarbage(T_CELL, GlobalEnv);
    INK(COLOR_INFO);
    puts("Expression aborted. GC completed; ready.");
    if (causa == RECURSO_SIMBOLOS)
        puts("GC does not free symbols. Use (NEW) to clear definitions.");
    INK(COLOR_ENTRADA);
}
