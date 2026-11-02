#ifndef GC_H
#define GC_H

#include "heap_ops.h"

#define UMBRAL_GC_REPL 80
#define UMBRAL_GC_LOAD 50

/* Solo tras volver al REPL, sin evaluaciones activas. No repite la expresion. */
void RecuperarRecursos(void);

/* Marca NIL, las dos raices recibidas y c1/c2/c3 de TODAS las tareas
   activas (tambien las de evaluaciones exteriores). Despues barre y
   compacta. Puntos admitidos: REPL, fin de una expresion de LOAD y
   CLEAN. No invocar desde NewCell ni desde constructores/builtins con
   temporales vivos solo en C: esos puntos requeririan mas raices.
   El marcado aun usa recursion para CAR/cuerpos anidados. */
void CollectGarbage(CellIdx raizExtra1, CellIdx raizExtra2);

/* 1 si conviene disparar el GC ahora: LiveUsagePercent >= umbral, O
   nstrs >= umbral% de MAXSTR (la doble condicion evita que programas
   pesados en strings agoten strtab sin que el heap principal lo note).
   Umbrales actuales: 80 en el REPL y 50 durante LOAD. */
int DeberiaDispararseGC(int umbralPorcentaje);

long LiveUsagePercent(void);   /* HeapUsagePercent menos lo que hay en freelist */

#endif