#ifndef ARITMETICA_H
#define ARITMETICA_H

#include "heap_ops.h"

/* Operaciones de Aritmetica() */
#define OP_SUMA  0
#define OP_RESTA 1
#define OP_MULT  2
#define OP_DIV   3

/* + - * / con enteros, racionales exactos y flotantes (ver aritmetica.c).
   Levanta EvalError y devuelve NIL_CELL ante error o desbordamiento. */
CellIdx Aritmetica(CellIdx args, int op);

/* Compara dos numeros: deja -1/0/1 en *cmp y devuelve 1; 0 si hubo error
   (ya mostrado y con EvalError levantado). */
int CompararNumeros(CellIdx ca, CellIdx cb, int *cmp);

#endif
