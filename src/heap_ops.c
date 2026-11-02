#include <string.h>
#include "heap_ops.h"

/* --- symtab/strtab paginadas (Tecnica L1 de Level9) ---
   offset dentro de PAGINA_TABLAS: symtab ocupa [0, MAXSYM*(SYMLEN+1)),
   strtab ocupa justo detras. Ambas caben en los 8K de una sola pagina
   (512*9 + 90*37 = 7.938 de 8.192 bytes). */
#define OFFSET_SYMTAB 0
#define OFFSET_STRTAB (MAXSYM * (SYMLEN + 1))

unsigned int nsyms = 0;
unsigned int nstrs = 0;

CellIdx NIL_CELL;
int ErrorFlag = 0;
int HeapError = 0;

#define FREELIST_NONE ((CellIdx)0xFFFFU)

void SymtabGet(unsigned int i, char destino[SYMLEN + 1]) {
    tabla_get_bytes(OFFSET_SYMTAB + i * (SYMLEN + 1),
                     (unsigned char *)destino, SYMLEN + 1);
}

void StrtabGet(unsigned int i, char destino[MAXSTRLEN + 1]) {
    tabla_get_bytes(OFFSET_STRTAB + i * (MAXSTRLEN + 1),
                     (unsigned char *)destino, MAXSTRLEN + 1);
}

void StrtabPut(unsigned int i, const char origen[MAXSTRLEN + 1]) {
    tabla_put_bytes(OFFSET_STRTAB + i * (MAXSTRLEN + 1),
                     (const unsigned char *)origen, MAXSTRLEN + 1);
}

/* --- helpers de empaquetado little-endian ---
   No reentrantes (no se llaman a si mismas ni entre ellas de forma
   recursiva) -- locales static seguros. */
static void put16(unsigned char *b, unsigned int v) {
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
}
static unsigned int get16(const unsigned char *b) {
    return (unsigned int)b[0] | ((unsigned int)b[1] << 8);
}
static void put32(unsigned char *b, long v) {
    b[0] = (unsigned char)(v & 0xFF);
    b[1] = (unsigned char)((v >> 8) & 0xFF);
    b[2] = (unsigned char)((v >> 16) & 0xFF);
    b[3] = (unsigned char)((v >> 24) & 0xFF);
}
static long get32(const unsigned char *b) {
    unsigned long u = (unsigned long)b[0] | ((unsigned long)b[1] << 8) |
                       ((unsigned long)b[2] << 16) | ((unsigned long)b[3] << 24);
    return (long)u;
}

static CellIdx freelist = FREELIST_NONE;
static CellIdx nextfree = 0;
static long celdasLibres = 0;   /* contador O(1) de celdas en freelist */

/* Buffer de 8 bytes (TAM_CELDA) COMPARTIDO por todas las funciones de
   este fichero que operan sobre una sola celda a la vez (constructores
   y accesores). Antes cada una declaraba su propio "static unsigned
   char bytes[TAM_CELDA]" -- 21 copias identicas, 21*8=168 bytes de BSS
   para un dato que nunca vive mas alla de la propia llamada. Seguro
   porque ninguna de estas funciones llama a otra del mismo grupo
   MIENTRAS su propio "bytes" sigue conteniendo datos que todavia
   necesita: los constructores compuestos (MakeRat, Cons dentro de
   MakeRat, etc.) siempre completan sus llamadas internas a NewCell/
   Cons/MakeInt ANTES de tocar su propio "bytes". */
static unsigned char bytes[TAM_CELDA];

void heap_ops_init(void) {
    nsyms = 0;
    nstrs = 0;
    ErrorFlag = 0;
    HeapError = 0;
    freelist = FREELIST_NONE;
    nextfree = 0;
    celdasLibres = 0;

    NIL_CELL = NewCell();
    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TNIL;
    heap_put_cell(NIL_CELL, bytes);
}

CellIdx NewCell(void) {
    static CellIdx idx;

    if (HeapError) return NIL_CELL;
    if (freelist != FREELIST_NONE) {
        idx = freelist;
        heap_get_cell(idx, bytes);
        freelist = get16(&bytes[3]);   /* enlace de la lista libre, en la posicion de cdr */
        celdasLibres--;
        return idx;
    }
    if (nextfree >= MAXCELL) {
        HeapError = RECURSO_CELDAS;
        ErrorFlag = 1;
        return NIL_CELL;
    }
    idx = nextfree;
    nextfree++;
    return idx;
}

CellIdx Cons(CellIdx a, CellIdx b) {
    CellIdx idx = NewCell();

    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TPAIR;
    put16(&bytes[1], a);
    put16(&bytes[3], b);
    heap_put_cell(idx, bytes);
    return idx;
}

CellIdx MakeInt(long v) {
    CellIdx idx = NewCell();

    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TINT;
    put32(&bytes[1], v);
    heap_put_cell(idx, bytes);
    return idx;
}

#ifndef MYLISP_SIN_FLOTANTES
CellIdx MakeFloat(float v) {
    CellIdx idx = NewCell();
    const unsigned char *raw = (const unsigned char *)&v;

    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TFLOAT;
    memcpy(&bytes[1], raw, 6);   /* sizeof(float) == 6 confirmado */
    heap_put_cell(idx, bytes);
    return idx;
}
#endif

CellIdx MakeRat(long n, long d) {
    CellIdx numcell, dencell, inner, idx;

    numcell = MakeInt(n);
    if (HeapError) return NIL_CELL;
    dencell = MakeInt(d);
    if (HeapError) return NIL_CELL;
    inner = Cons(numcell, dencell);
    if (HeapError) return NIL_CELL;

    idx = NewCell();
    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TRAT;
    put16(&bytes[1], inner);
    heap_put_cell(idx, bytes);
    return idx;
}

CellIdx MakeString(const char *s) {
    CellIdx idx;
    static char tmp[MAXSTRLEN + 1];
    unsigned int stridx;

    if (HeapError) return NIL_CELL;
    if (nstrs >= MAXSTR) {
        HeapError = RECURSO_CADENAS;
        ErrorFlag = 1;
        return NIL_CELL;
    }
    stridx = nstrs;
    strncpy(tmp, s, MAXSTRLEN);
    tmp[MAXSTRLEN] = '\0';
    StrtabPut(stridx, tmp);
    nstrs++;

    idx = NewCell();
    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TSTRING;
    put16(&bytes[1], stridx);
    heap_put_cell(idx, bytes);
    return idx;
}

CellIdx MakeSym(unsigned int symidx) {
    CellIdx idx = NewCell();

    if (HeapError) return NIL_CELL;

    memset(bytes, 0, TAM_CELDA);
    bytes[0] = TSYM;
    put16(&bytes[1], symidx);
    heap_put_cell(idx, bytes);
    return idx;
}

CellIdx MakeClosure(CellIdx parametros, CellIdx cuerpo, CellIdx entorno) {
    CellIdx idx = NewCell();

    if (HeapError) return NIL_CELL;

    bytes[0] = TCLOSURE;
    put16(&bytes[1], parametros);
    put16(&bytes[3], cuerpo);
    put16(&bytes[5], entorno);
    bytes[7] = 0;   /* mark */
    heap_put_cell(idx, bytes);
    return idx;
}

unsigned int InternSym(const char *name) {
    static unsigned int i;
    static char truncado[SYMLEN + 1];
    unsigned char *ventana;
    unsigned int resultado;

    if (HeapError) return 0;
    strncpy(truncado, name, SYMLEN);
    truncado[SYMLEN] = '\0';

    /* Antes: un acceso paginado (SymtabGet) POR CADA comparacion --
       hasta nsyms accesos completos solo para reencontrar un simbolo
       ya conocido. Medido empiricamente: volver a resolver "CONS" 2000
       veces costaba ~18s frente a ~2s del mismo trabajo de heap sin
       pasar por el lexer/parser -- la busqueda lineal aqui era la
       causa principal. Ahora: UN solo cambio de pagina para toda la
       busqueda (y la escritura si hace falta internar uno nuevo),
       comparando directamente contra la memoria ya mapeada. */
    ventana = heap_page_in(PAGINA_TABLAS);

    for (i = 0; i < nsyms; i++) {
        if (strcmp((char *)(ventana + OFFSET_SYMTAB + i * (SYMLEN + 1)),
                   truncado) == 0) {
            heap_page_out();
            return i;
        }
    }
    if (nsyms >= MAXSYM) {
        heap_page_out();
        HeapError = RECURSO_SIMBOLOS;
        ErrorFlag = 1;
        return 0;   /* fallback, igual de espiritu que el "InternSym := 1" de Pascal */
    }
    strcpy((char *)(ventana + OFFSET_SYMTAB + nsyms * (SYMLEN + 1)), truncado);
    resultado = nsyms;
    nsyms++;
    heap_page_out();
    return resultado;
}

unsigned char GetTag(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return bytes[0];
}

CellIdx GetCar(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return (CellIdx)get16(&bytes[1]);
}

CellIdx GetCdr(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return (CellIdx)get16(&bytes[3]);
}

long GetIntValue(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return get32(&bytes[1]);
}

#ifndef MYLISP_SIN_FLOTANTES
float GetFloatValue(CellIdx idx) {
    float v;
    unsigned char *raw = (unsigned char *)&v;

    heap_get_cell(idx, bytes);
    memcpy(raw, &bytes[1], 6);
    return v;
}
#endif

unsigned int GetSymIdx(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return get16(&bytes[1]);
}

unsigned int GetStrIdx(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return get16(&bytes[1]);
}

CellIdx GetClosureParams(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return (CellIdx)get16(&bytes[1]);
}

CellIdx GetClosureBody(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return (CellIdx)get16(&bytes[3]);
}

CellIdx GetClosureEnv(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return (CellIdx)get16(&bytes[5]);
}

void SetCar(CellIdx idx, CellIdx nuevoCar) {
    heap_get_cell(idx, bytes);
    put16(&bytes[1], nuevoCar);
    heap_put_cell(idx, bytes);
}

unsigned char GetMark(CellIdx idx) {
    heap_get_cell(idx, bytes);
    return bytes[7];
}

void SetMark(CellIdx idx, unsigned char valor) {
    heap_get_cell(idx, bytes);
    bytes[7] = valor;
    heap_put_cell(idx, bytes);
}

CellIdx HeapNextFree(void) {
    return nextfree;
}

/* FASE 2 del GC (barrido): reconstruye freelist recorriendo [0, nextfree)
   y encadenando las celdas SIN marca -- exactamente igual que hace
   NewCell() para consumirlas despues (enlace en la posicion de cdr). */
/* FASE 1 del GC (limpiar marcas): recorre [0, tope) SECUENCIALMENTE,
   asi que podemos paginar una pagina de 1024 celdas de una vez y
   escribir directamente en la ventana mapeada -- un cambio de MMU2
   por cada 1024 celdas, en vez de uno por celda. */
void HeapClearMarks(CellIdx tope) {
    unsigned int paginaActual = 0xFFFFU;
    unsigned int paginaNecesaria;
    unsigned char *ventana = 0;
    CellIdx i;

    for (i = 0; i < tope; i++) {
        paginaNecesaria = PRIMERA_PAGINA_HEAP + (i >> 10);
        if (paginaNecesaria != paginaActual) {
            if (ventana) heap_page_out();
            ventana = heap_page_in(paginaNecesaria);
            paginaActual = paginaNecesaria;
        }
        ventana[((i & 1023) << 3) + 7] = 0;   /* byte de marca, offset 7 */
    }
    if (ventana) heap_page_out();
}

/* FASE 2 del GC (barrido): misma tecnica -- recorrido secuencial,
   un cambio de MMU2 por pagina en vez de por celda. Antes de este
   cambio, cada celda costaba heap_get_cell (que a su vez llamaba a
   GetMark, otro heap_get_cell) + heap_put_cell: hasta TRES cambios de
   banco por celda. Ahora es un acceso de memoria normal mientras la
   pagina esta puesta. */
void HeapSweep(void) {
    unsigned int paginaActual = 0xFFFFU;
    unsigned int paginaNecesaria;
    unsigned char *ventana = 0;
    unsigned int offset;
    CellIdx i;

    freelist = FREELIST_NONE;
    celdasLibres = 0;

    for (i = 0; i < nextfree; i++) {
        paginaNecesaria = PRIMERA_PAGINA_HEAP + (i >> 10);
        if (paginaNecesaria != paginaActual) {
            if (ventana) heap_page_out();
            ventana = heap_page_in(paginaNecesaria);
            paginaActual = paginaNecesaria;
        }
        offset = (i & 1023) << 3;
        if (ventana[offset + 7] == 0) {   /* sin marca */
            ventana[offset] = TFREE;
            ventana[offset + 3] = (unsigned char)(freelist & 0xFF);
            ventana[offset + 4] = (unsigned char)((freelist >> 8) & 0xFF);
            freelist = i;
            celdasLibres++;
        }
    }
    if (ventana) heap_page_out();
}

long HeapFreeCount(void) {
    return celdasLibres;
}