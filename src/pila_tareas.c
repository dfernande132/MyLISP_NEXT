#include <string.h>
#include "pila_tareas.h"
#include "heap_paged.h"

/* Paginas 82-89: justo DESPUES del rango que ocupa el heap principal
   (50-81, para MAXCELL=32768 celdas a 1024/pagina). Elegidas a
   proposito contiguas a un rango que YA sabemos que funciona, en vez
   de un numero de pagina sin probar -- version anterior (40-47) causo
   una pantalla corrupta/colgada, probablemente por colisionar con
   algo que NextZXOS necesita para si mismo en esa zona. */
#define PRIMERA_PAGINA_PILA 82
#define TAREAS_POR_PAGINA   1024
#define TAM_TAREA           8
#define MAX_TAREAS          8192   /* 8 paginas * 1024 */

static unsigned int tope = 0;   /* indice de la PRIMERA tarea libre (tope de la pila) */

unsigned int PilaTareasProfundidadMaxima = 0;

/* El GC puede leer cualquier tarea sin alterar la pila. Usar las
   primitivas residentes comunes evita duplicar DI/MMU2/restauracion. */
void PilaTareasLeer(unsigned int idx, unsigned char bytes[TAM_TAREA]) {
    unsigned int pagina = PRIMERA_PAGINA_PILA + (idx >> 10);
    unsigned int offset = (idx & 1023) << 3;
    unsigned char *p = heap_page_in(pagina) + offset;

    memcpy(bytes, p, TAM_TAREA);
    heap_page_out();
}

static void tarea_put(unsigned int idx, const unsigned char bytes[TAM_TAREA]) {
    unsigned int pagina = PRIMERA_PAGINA_PILA + (idx >> 10);
    unsigned int offset = (idx & 1023) << 3;
    unsigned char *p = heap_page_in(pagina) + offset;

    memcpy(p, bytes, TAM_TAREA);
    heap_page_out();
}

static void empaquetar(unsigned char bytes[TAM_TAREA], unsigned char kind,
                        CellIdx c1, CellIdx c2, CellIdx c3) {
    bytes[0] = kind;
    bytes[1] = (unsigned char)(c1 & 0xFF);
    bytes[2] = (unsigned char)((c1 >> 8) & 0xFF);
    bytes[3] = (unsigned char)(c2 & 0xFF);
    bytes[4] = (unsigned char)((c2 >> 8) & 0xFF);
    bytes[5] = (unsigned char)(c3 & 0xFF);
    bytes[6] = (unsigned char)((c3 >> 8) & 0xFF);
    bytes[7] = 0;
}

void PilaTareasInit(void) {
    tope = 0;
    PilaTareasProfundidadMaxima = 0;
}

unsigned int PilaTareasTope(void) {
    return tope;
}

void PilaTareasEmpujar(unsigned char kind, CellIdx c1, CellIdx c2, CellIdx c3) {
    static unsigned char bytes[TAM_TAREA];

    if (HeapError) return;
    if (tope >= MAX_TAREAS) {
        HeapError = RECURSO_TAREAS;
        ErrorFlag = 1;
        return;
    }
    empaquetar(bytes, kind, c1, c2, c3);
    tarea_put(tope, bytes);
    tope++;
    if (tope > PilaTareasProfundidadMaxima) PilaTareasProfundidadMaxima = tope;
}

void PilaTareasMutarCima(unsigned char kind, CellIdx c1, CellIdx c2, CellIdx c3) {
    static unsigned char bytes[TAM_TAREA];

    empaquetar(bytes, kind, c1, c2, c3);
    tarea_put(tope - 1, bytes);
}

void PilaTareasDesapilar(void) {
    if (tope > 0) tope--;
}

void PilaTareasLeerCima(unsigned char *kind, CellIdx *c1, CellIdx *c2, CellIdx *c3) {
    static unsigned char bytes[TAM_TAREA];
    PilaTareasLeer(tope - 1, bytes);
    *kind = bytes[0];
    *c1 = (CellIdx)((unsigned int)bytes[1] | ((unsigned int)bytes[2] << 8));
    *c2 = (CellIdx)((unsigned int)bytes[3] | ((unsigned int)bytes[4] << 8));
    *c3 = (CellIdx)((unsigned int)bytes[5] | ((unsigned int)bytes[6] << 8));
}