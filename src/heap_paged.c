#include "diag_pila.h"
#include <string.h>
#include <arch/zxn.h>
#include <intrinsic.h>
#include "heap_paged.h"

/* CELDAS_POR_PAGINA (1024) y TAM_CELDA (8) son potencias de 2 -- se
   escriben como desplazamientos (>>10, <<3) en vez de / y * para
   garantizar que el compilador nunca genere una llamada a la rutina
   generica de division/multiplicacion de 16 bits. sccz80 no hace esta
   optimizacion automaticamente (confirmado en la wiki "WritingOptimalCode":
   "no realiza ninguna optimizacion mas alla de un plegado de constantes
   limitado"), y esta funcion se ejecuta en CADA acceso al heap. */

void heap_put_cell(unsigned int idx, const unsigned char bytes[TAM_CELDA]) {
    static unsigned int pagina;
    static unsigned int offset;
    unsigned char *p;

    pagina = PRIMERA_PAGINA_HEAP + (idx >> 10);      /* idx / 1024 */
    offset = (idx & 1023) << 3;                       /* (idx % 1024) * 8 */
    p = (unsigned char *)(VENTANA_HEAP_BASE + offset);

    DIAG_PILA();
    intrinsic_di();
    ZXN_WRITE_MMU2(pagina);
    memcpy(p, bytes, TAM_CELDA);
    ZXN_WRITE_MMU2(PAGINA_PANTALLA_DEFECTO);
    intrinsic_ei();
}

void heap_get_cell(unsigned int idx, unsigned char bytes[TAM_CELDA]) {
    static unsigned int pagina;
    static unsigned int offset;
    unsigned char *p;

    pagina = PRIMERA_PAGINA_HEAP + (idx >> 10);
    offset = (idx & 1023) << 3;
    p = (unsigned char *)(VENTANA_HEAP_BASE + offset);

    DIAG_PILA();
    intrinsic_di();
    ZXN_WRITE_MMU2(pagina);
    memcpy(bytes, p, TAM_CELDA);
    ZXN_WRITE_MMU2(PAGINA_PANTALLA_DEFECTO);
    intrinsic_ei();
}

/* --- pagina de tablas (symtab+strtab), Tecnica L1 de Level9 --- */

void tabla_get_bytes(unsigned int offset, unsigned char *destino, unsigned int len) {
    unsigned char *p = (unsigned char *)(VENTANA_HEAP_BASE + offset);

    DIAG_PILA();
    intrinsic_di();
    ZXN_WRITE_MMU2(PAGINA_TABLAS);
    memcpy(destino, p, len);
    ZXN_WRITE_MMU2(PAGINA_PANTALLA_DEFECTO);
    intrinsic_ei();
}

void tabla_put_bytes(unsigned int offset, const unsigned char *origen, unsigned int len) {
    unsigned char *p = (unsigned char *)(VENTANA_HEAP_BASE + offset);

    DIAG_PILA();
    intrinsic_di();
    ZXN_WRITE_MMU2(PAGINA_TABLAS);
    memcpy(p, origen, len);
    ZXN_WRITE_MMU2(PAGINA_PANTALLA_DEFECTO);
    intrinsic_ei();
}

/* --- paginado por bloques (para recorridos secuenciales del GC) --- */

unsigned char *heap_page_in(unsigned int pagina) __z88dk_fastcall {
    DIAG_PILA();
    intrinsic_di();
    ZXN_WRITE_MMU2(pagina);
    return (unsigned char *)VENTANA_HEAP_BASE;
}

void heap_page_out(void) {
    ZXN_WRITE_MMU2(PAGINA_PANTALLA_DEFECTO);
    intrinsic_ei();
}