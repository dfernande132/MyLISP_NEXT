#ifndef HEAP_OPS_H
#define HEAP_OPS_H

#include "heap_paged.h"

/* --- tags de celda (constantes en vez de enum, para controlar el byte
   almacenado con precision -- mismo orden que TagType en MYLISP.pas) --- */
#define TPAIR    0
#define TSYM     1
#define TINT     2
#define TRAT     3
#define TFLOAT   4
#define TNIL     5
#define TFREE    6
#define TCLOSURE 7
#define TSTRING  8

typedef unsigned int CellIdx;   /* indice de celda; 0 reservado para NIL_CELL */

#define MAXSYM 512   /* simbolos distintos; tablas juntas caben en pagina 49 */
#define SYMLEN 8
#define MAXSTR 90    /* entradas de tabla de strings */
#define MAXSTRLEN 36

#if MAXSYM * (SYMLEN + 1) + MAXSTR * (MAXSTRLEN + 1) > 8192
#error Tablas de simbolos y cadenas exceden la pagina de 8 KB
#endif

/* symtab y strtab YA NO son arrays residentes en la zona base -- viven
   en PAGINA_TABLAS (ver heap_paged.h), siguiendo la Tecnica L1 de
   Level9 ("effective()"). Se accede a ellas por indice, nunca por
   puntero directo -- de ahi que ya no se declaren "extern char [][]"
   aqui. Ambas tablas ocupan MAXSYM*(SYMLEN+1) + MAXSTR*(MAXSTRLEN+1). */
void SymtabGet(unsigned int i, char destino[SYMLEN + 1]);
void StrtabGet(unsigned int i, char destino[MAXSTRLEN + 1]);
void StrtabPut(unsigned int i, const char origen[MAXSTRLEN + 1]);

extern unsigned int nsyms;
extern unsigned int nstrs;

extern CellIdx NIL_CELL;
extern int ErrorFlag;
/* Causa de agotamiento; cero significa sin error. */
#define RECURSO_CELDAS 1
#define RECURSO_SIMBOLOS 2
#define RECURSO_CADENAS 3
#define RECURSO_TAREAS 4
extern int HeapError;

void heap_ops_init(void);

/* --- gestion basica --- */
CellIdx NewCell(void);

/* --- constructores --- */
CellIdx Cons(CellIdx a, CellIdx b);
CellIdx MakeInt(long v);
#ifndef MYLISP_SIN_FLOTANTES
CellIdx MakeFloat(float v);
#endif
CellIdx MakeRat(long n, long d);
CellIdx MakeString(const char *s);
CellIdx MakeSym(unsigned int symidx) __z88dk_fastcall;
CellIdx MakeClosure(CellIdx parametros, CellIdx cuerpo, CellIdx entorno);

/* --- tabla de simbolos --- */
unsigned int InternSym(const char *name) __z88dk_fastcall;

/* --- accesores de lectura --- */
/* Estas son, con diferencia, las funciones mas llamadas de todo el
   interprete (cada Eval, cada PrintExpr, cada paso de GC futuro pasa
   por aqui). __z88dk_fastcall pasa el unico parametro en un registro
   en vez de por la pila -- ahorra en cada una de esas miles de
   llamadas. Solo aplica a parametros que caben en 16 bits (CellIdx,
   punteros); MakeInt/MakeFloat NO son candidatas porque sus
   parametros (long de 32 bits, float de 6 bytes) no caben en un
   registro. */
unsigned char GetTag(CellIdx idx) __z88dk_fastcall;
CellIdx       GetCar(CellIdx idx) __z88dk_fastcall;
CellIdx       GetCdr(CellIdx idx) __z88dk_fastcall;
long          GetIntValue(CellIdx idx) __z88dk_fastcall;
#ifndef MYLISP_SIN_FLOTANTES
float         GetFloatValue(CellIdx idx) __z88dk_fastcall;
#endif
unsigned int  GetSymIdx(CellIdx idx) __z88dk_fastcall;
unsigned int  GetStrIdx(CellIdx idx) __z88dk_fastcall;
CellIdx       GetClosureParams(CellIdx idx) __z88dk_fastcall;
CellIdx       GetClosureBody(CellIdx idx) __z88dk_fastcall;
CellIdx       GetClosureEnv(CellIdx idx) __z88dk_fastcall;

/* Mutacion de bajo nivel -- solo para uso interno del entorno global
   (replica el "carf[GlobalEnv] := ..." de MYLISP.pas). NUNCA se expone
   al lenguaje Lisp; el resto de MyLISP sigue siendo deliberadamente
   inmutable (sin SETQ). */
void SetCar(CellIdx idx, CellIdx nuevoCar);

/* --- soporte del GC mark-sweep ---
   El byte de "mark" (bytes[7] de cada celda, puesto a 0 por todos los
   constructores) vive DENTRO de la celda paginada -- no hace falta un
   array aparte de MAXCELL bytes, que seria inviable en 32K. */
unsigned char GetMark(CellIdx idx) __z88dk_fastcall;
void SetMark(CellIdx idx, unsigned char valor);

/* freelist/nextfree siguen encapsulados (static) dentro de heap_ops.c;
   el GC solo necesita saber hasta donde recorrer y un modo de
   reconstruir la lista libre a partir de las marcas ya puestas. */
CellIdx HeapNextFree(void);
void HeapSweep(void);
void HeapClearMarks(CellIdx tope);   /* fase 1 del GC, por bloques de pagina entera */

/* Contador de celdas libres mantenido EN TIEMPO REAL (O(1) para
   leerlo) -- se actualiza en NewCell() y en HeapSweep(), en vez de
   recorrer el heap contando celdas TFREE cada vez que se pregunta
   (eso era O(n) accesos paginados por consulta, y DeberiaDispararseGC
   se llama con frecuencia -- un fallo de diseno real, encontrado
   empiricamente al medir cuanto tardaba la prueba del GC). */
long HeapFreeCount(void);

#endif