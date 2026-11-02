#ifndef PILA_TAREAS_H
#define PILA_TAREAS_H

#include "heap_ops.h"

/* Tipos de tarea del evaluador iterativo. Prototipo minimo: solo los
   necesarios para atomos/simbolos/aplicacion de funcion. IF/COND/
   SECUENCIA (PROGN/LET/cuerpo de LAMBDA) se anaden despues, una vez
   probado el mecanismo basico con recursion de verdad profunda. */
#define TAREA_EVAL_EXPR    0   /* c1=expr, c2=env */
#define TAREA_EVAL_ARGS    1   /* c1=pendientes, c2=evaluados(invertidos), c3=env */
#define TAREA_APLICAR_FN   2   /* c1=argsExpr(resto sin evaluar), c2=env -- esperando el valor de la funcion */
#define TAREA_APLICAR_ARGS 3   /* c1=fn, c2=env -- esperando la lista de argumentos evaluada */
#define TAREA_IF_ESPERANDO 4   /* c1=ramaThen, c2=restoRamas (else, o NIL), c3=env -- esperando el valor del test */
#define TAREA_APLICAR_BUILTIN 5   /* c1=simbolo(celda TSYM), c2=env -- esperando la lista de argumentos evaluada */
#define TAREA_SECUENCIA 6         /* c1=expresiones pendientes, c2=env -- PROGN/cuerpo de funcion/cuerpo de LET */
#define TAREA_LET_ESPERANDO_BINDINGS 7   /* c1=cuerpo, c2=envOriginal -- esperando el frame de bindings ya evaluado */
#define TAREA_LET_BINDINGS 8      /* c1=bindings pendientes (SIN avanzar hasta que vuelve el valor), c2=frame acumulado, c3=envOriginal */
#define TAREA_AND_ESPERANDO 9     /* c1=pendientes, c2=env */
#define TAREA_OR_ESPERANDO 10     /* c1=pendientes, c2=env */
#define TAREA_COND_ESPERANDO 11   /* c1=clausulaActual, c2=clausulasRestantes, c3=env */
#define TAREA_DEFINE_ESPERANDO 12 /* c1=nombre */

void PilaTareasInit(void);
unsigned int PilaTareasTope(void);   /* cuantas tareas hay puestas ahora mismo -- para llamadas anidadas */

void PilaTareasEmpujar(unsigned char kind, CellIdx c1, CellIdx c2, CellIdx c3);
void PilaTareasMutarCima(unsigned char kind, CellIdx c1, CellIdx c2, CellIdx c3);
void PilaTareasDesapilar(void);

/* lee kind+c1+c2+c3 de la cima con UN solo acceso paginado, en vez de
   hasta cuatro (una llamada, un DI/cambio de MMU2/copia/restaurar/EI
   en total) -- eval_iterativo.c la usa en el bucle principal, que se
   ejecuta muchisimas veces por cada evaluacion */
/* Copia cruda de una tarea valida (idx < PilaTareasTope), sin sacarla.
   Bytes 1-6: c1/c2/c3 little endian; todos son referencias o cero en
   los tipos actuales. La ventana de datos queda restaurada al salir. */
void PilaTareasLeer(unsigned int idx, unsigned char bytes[8]);

void PilaTareasLeerCima(unsigned char *kind, CellIdx *c1, CellIdx *c2, CellIdx *c3);

/* diagnostico: profundidad maxima de tareas alcanzada, para calibrar
   cuantas paginas hacen falta de verdad */
extern unsigned int PilaTareasProfundidadMaxima;

#endif