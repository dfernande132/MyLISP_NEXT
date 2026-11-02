#include "diag_pila.h"
#include "gc.h"
#include "pila_tareas.h"

/* Seguir CDR/entorno como un bucle: la longitud de una lista plana
   ya no anade marcos de C. La recursion por CAR/cuerpos anidados sigue
   existiendo y se tratara por separado. Marcar antes de seguir aristas
   permite terminar tambien cuando hay ciclos de clausuras/entornos. */
static void MarkCell(CellIdx idx) {
    unsigned char t;
    DIAG_PILA();

    while (idx < HeapNextFree() && !GetMark(idx)) {
        SetMark(idx, 1);
        t = GetTag(idx);
        switch (t) {
            case TPAIR:
                MarkCell(GetCar(idx));
                idx = GetCdr(idx);
                break;
            case TRAT:
                /* El par interno ya enlaza numerador y denominador. */
                idx = GetCar(idx);
                break;
            case TCLOSURE:
                MarkCell(GetClosureParams(idx));
                MarkCell(GetClosureBody(idx));
                idx = GetClosureEnv(idx);
                break;
            default:
                return;
        }
    }
}

/* FASE 4: compactar sin destruir cadenas pendientes de copiar.
   Tras reciclar celdas, el orden fisico del heap NO coincide con el
   de strtab. Primero identificar indices vivos, despues copiar por
   indice ORIGINAL ascendente y por ultimo actualizar las celdas.
   Cada destino es <= origen: ninguna copia pisa un origen futuro.
   Dos barridos del heap, paginados por bloques; mismos buffers BSS. */
static void CompactarStrtab(void) {
    static unsigned char strMap[MAXSTR];
    static char buffer[MAXSTRLEN + 1];
    CellIdx tope = HeapNextFree();
    CellIdx i;
    unsigned int stridx, contador;
    unsigned int paginaActual;
    unsigned int paginaNecesaria, offset;
    unsigned char *ventana;
    unsigned char fase;

    for (i = 0; i < MAXSTR; i++) strMap[i] = 0xFF;

    for (fase = 0; fase < 2; fase++) {
        paginaActual = 0xFFFFU;
        ventana = 0;
        for (i = 0; i < tope; i++) {
            paginaNecesaria = PRIMERA_PAGINA_HEAP + (i >> 10);
            if (paginaNecesaria != paginaActual) {
                if (ventana) heap_page_out();
                ventana = heap_page_in(paginaNecesaria);
                paginaActual = paginaNecesaria;
            }
            offset = (i & 1023) << 3;
            if (ventana[offset + 7] != 0 && ventana[offset] == TSTRING) {
                stridx = (unsigned int)ventana[offset + 1] |
                         ((unsigned int)ventana[offset + 2] << 8);
                if (fase == 0) {
                    strMap[stridx] = 0;  /* vivo, aun sin nuevo indice */
                } else {
                    ventana[offset + 1] = strMap[stridx];
                    ventana[offset + 2] = 0;  /* MAXSTR < 256 */
                }
            }
        }
        if (ventana) heap_page_out();

        if (fase == 0) {
            contador = 0;
            for (stridx = 0; stridx < nstrs; stridx++) {
                if (strMap[stridx] != 0xFF) {
                    StrtabGet(stridx, buffer);
                    StrtabPut(contador, buffer);
                    strMap[stridx] = (unsigned char)contador;
                    contador++;
                }
            }
            nstrs = contador;
        }
    }
}

void CollectGarbage(CellIdx raizExtra1, CellIdx raizExtra2) {
    CellIdx tope = HeapNextFree();
    unsigned int i;
    unsigned char j;
    static unsigned char tarea[8];

    /* FASE 1: limpiar marcas (por bloques de pagina, ver heap_ops.c)
       y marcar lo alcanzable desde las raices (esto si es celda a
       celda, porque sigue punteros arbitrarios, no es secuencial) */
    HeapClearMarks(tope);
    MarkCell(NIL_CELL);
    MarkCell(raizExtra1);
    MarkCell(raizExtra2);

    /* Contrato de los 13 tipos actuales: c1/c2/c3 son CellIdx o 0.
       Leer una COPIA antes de marcar, porque el marcado pagina MMU2.
       No se cambia el tope: incluye contextos exteriores de EVAL/LOAD.
       Si se anaden campos escalares, revisar este contrato. */
    for (i = 0; i < PilaTareasTope(); i++) {
        PilaTareasLeer(i, tarea);
        for (j = 1; j < 7; j += 2) {
            MarkCell((CellIdx)((unsigned int)tarea[j] |
                              ((unsigned int)tarea[j + 1] << 8)));
        }
    }

    /* FASE 2: barrer el heap principal (delegado en heap_ops.c, por
       bloques de pagina) */
    HeapSweep();

    /* FASE 3 (coma flotante): omitida mientras MYLISP_SIN_FLOTANTES
       este activo -- no hay celdas TFLOAT que compactar */

    /* FASE 4: compactar strtab */
    CompactarStrtab();
}

int DeberiaDispararseGC(int umbralPorcentaje) {
    long umbralStrings = ((long)MAXSTR * umbralPorcentaje) / 100;
    return (LiveUsagePercent() >= umbralPorcentaje) ||
           ((long)nstrs >= umbralStrings);
}

long LiveUsagePercent(void) {
    return (((long)HeapNextFree() - HeapFreeCount()) * 100) / MAXCELL;
}