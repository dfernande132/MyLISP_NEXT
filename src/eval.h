#ifndef EVAL_H
#define EVAL_H

#include "heap_ops.h"

extern int EvalError;
extern CellIdx GlobalEnv;
extern CellIdx T_CELL;

void eval_init(void);

/* entorno: lista de frames (Cons), cada frame una lista de ligaduras
   (sym . valor) -- fiel a la estructura de dos niveles de MYLISP.pas.
   GlobalEnv se crea UNA VEZ y se muta en el sitio (via SetCar) en
   cada DEFINE, nunca se reasigna -- asi las clausuras que capturaron
   el entorno global ven las definiciones posteriores (incluida la
   suya propia, necesario para la recursion). */
CellIdx EnvLookup(CellIdx env, unsigned int symidx);
CellIdx ExtendEnv(CellIdx params, CellIdx args, CellIdx env);
void    DefineGlobal(unsigned int symidx, CellIdx valor);

int IsTruthy(CellIdx v) __z88dk_fastcall;

/* 1 si v es el simbolo VOID (lo devuelven DISPLAY/NEWLINE) -- el REPL
   y LOAD lo usan para no imprimir el eco del resultado cuando la
   propia forma ya imprimio algo, evitando el doble-print. */
int EsVoid(CellIdx v) __z88dk_fastcall;

/* Expuestas para el evaluador iterativo (eval_iterativo.c) -- los
   builtins (+, -, =, CAR...) no necesitan un mecanismo de tareas
   propio, se resuelven "de un golpe" una vez los argumentos estan
   evaluados, igual que en el evaluador recursivo. */
int EsBuiltin(unsigned int sym);
CellIdx AplicarBuiltin(unsigned int sym, CellIdx args);

/* Grupo G: sin subexpresiones que evaluar de forma diferida -- se
   resuelven "de un golpe", igual que un builtin. Expuestas para el
   evaluador iterativo. */
CellIdx EvalStatus(void);
CellIdx EvalClean(void);
CellIdx EvalNew(void);
CellIdx EvalSymbols(void);

CellIdx Eval(CellIdx expr, CellIdx env);
CellIdx Apply(CellIdx fn, CellIdx args, CellIdx env);
CellIdx EvalList(CellIdx lst, CellIdx env);

#endif