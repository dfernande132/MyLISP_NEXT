#ifndef EVAL_ITERATIVO_H
#define EVAL_ITERATIVO_H

#include "heap_ops.h"

/* Evaluador iterativo (prototipo minimo, sesion 20/07/2026): mismo
   resultado que Eval() para atomos/simbolos/aplicacion de funcion,
   pero SIN recursion de C -- toda la "profundidad" vive en la pila
   de tareas paginada (pila_tareas.h), no en SP. Limitacion deliberada
   de este prototipo: cuerpos de funcion de UNA sola expresion (sin
   secuencia tipo PROGN todavia), sin IF/COND/builtins -- se anaden
   despues de confirmar que el mecanismo aguanta recursion de verdad
   profunda. */
CellIdx EvalIterativo(CellIdx exprInicial, CellIdx envInicial);

/* Invalida la cache de simbolos especiales (ver Simbolo() en
   eval_iterativo.c) -- necesario cada vez que se reinterna todo desde
   cero (NEW), porque los indices de simbolo pueden cambiar y la cache
   seguiria devolviendo los viejos, ya invalidos. */
void EvalIterativoReset(void);

#endif