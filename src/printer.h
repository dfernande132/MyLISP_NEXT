#ifndef PRINTER_H
#define PRINTER_H

#include "heap_ops.h"

void PrintExpr(CellIdx idx) __z88dk_fastcall;

/* Tope de anidamiento por CAR al imprimir (~4 bytes de pila por nivel).
   Mas alla se escribe "..." en vez de seguir bajando. Las listas LARGAS no
   cuentan: el CDR se recorre con un bucle. */
#define PRINT_PROFUNDIDAD_MAX 64

#endif