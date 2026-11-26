#include "diag_pila.h"
#include "gc.h"
#include "pila_tareas.h"

/* Seguir CDR/entorno como un bucle: la longitud de una lista plana
   ya no anade marcos de C. La recursion por CAR/cuerpos anidados esta
   ACOTADA por PROF_MARCADO: al llegar al tope el hijo se deja sin
   marcar y se anota en marcaTruncada; RemarcarTruncados() lo recoge
   despues con pasadas sobre el heap (siempre correcto, solo mas lento
   en estructuras muy anidadas). Marcar antes de seguir aristas permite
   terminar tambien cuando hay ciclos de clausuras/entornos. */
#ifndef PROF_MARCADO
#define PROF_MARCADO 32
#endif

static unsigned char profMarca;
static unsigned char marcaTruncada;

static void MarkCell(CellIdx idx) {
    unsigned char t;
    DIAG_PILA();

    while (idx < HeapNextFree() && !GetMark(idx)) {
        SetMark(idx, 1);
        t = GetTag(idx);
        switch (t) {
            case TPAIR:
                if (profMarca < PROF_MARCADO) {
                    profMarca++;
                    MarkCell(GetCar(idx));
                    profMarca--;
                } else {
                    marcaTruncada = 1;
                }
                idx = GetCdr(idx);
                break;
            case TRAT:
                /* El par interno ya enlaza numerador y denominador. */
                idx = GetCar(idx);
                break;
            case TCLOSURE:
                if (profMarca < PROF_MARCADO) {
                    profMarca++;
                    MarkCell(GetClosureParams(idx));
                    MarkCell(GetClosureBody(idx));
                    profMarca--;
                } else {
                    marcaTruncada = 1;
                }
                idx = GetClosureEnv(idx);
                break;
            default:
                return;
        }
    }
}

/* Si el marcado se trunco, recorre el heap buscando celdas marcadas
   con hijos sin marcar y los marca partiendo de profundidad 0. Alterna
   sentido ascendente/descendente para converger rapido segun el orden
   de creacion de la estructura. Cada pasada con truncado marca al
   menos una celda nueva, asi que termina. */
static void RemarcarTruncados(void) {
    CellIdx tope, n, i;
    unsigned char descendente = 0;

    while (marcaTruncada) {
        marcaTruncada = 0;
        tope = HeapNextFree();
        for (n = 0; n < tope; n++) {
            i = descendente ? (CellIdx)(tope - 1 - n) : n;
            if (!GetMark(i)) continue;
            switch (GetTag(i)) {
                case TPAIR:
                    MarkCell(GetCar(i));
                    MarkCell(GetCdr(i));
                    break;
                case TCLOSURE:
                    MarkCell(GetClosureParams(i));
                    MarkCell(GetClosureBody(i));
                    MarkCell(GetClosureEnv(i));
                    break;
                default:
                    break;
            }
        }
        descendente = !descendente;
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
    profMarca = 0;
    marcaTruncada = 0;
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

    RemarcarTruncados();

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