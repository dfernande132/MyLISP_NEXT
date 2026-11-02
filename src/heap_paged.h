#ifndef HEAP_PAGED_H
#define HEAP_PAGED_H

#define PRIMERA_PAGINA_HEAP     50
#define CELDAS_POR_PAGINA       1024
#define TAM_CELDA               8
#define VENTANA_HEAP_BASE       0x4000   /* MMU 2, banco de pantalla ULA */
#define PAGINA_PANTALLA_DEFECTO 10       /* pagina 8K de la pantalla real (banco 5) */

/* Pagina dedicada a symtab+strtab (Tecnica L1 de Level9, "effective()" --
   ver TECNICAS_VFORTH_LEVEL9.md seccion Parte 2). Ambas tablas caben
   en un solo bloque de 8K (512*9 + 90*37 = 7.938 bytes
   de 8.192 disponibles), asi que basta una pagina, distinta de
   las del heap de celdas (50+) y de las de pantalla. */
#define PAGINA_TABLAS 49

/* 32 paginas completas (50-81): 32768 celdas de 8 bytes, indices
   0-32767. La siguiente pagina (82) sigue reservada a las tareas. */
#define MAXCELL 32768U

#if (MAXCELL + CELDAS_POR_PAGINA - 1) / CELDAS_POR_PAGINA > 32
#error El heap invade las paginas reservadas a la pila de tareas
#endif

/* --- primitivas de bajo nivel: leen/escriben una celda completa de
   TAM_CELDA bytes en bruto. heap_ops.c interpreta esos bytes segun
   el tag (byte 0). Protegidas con DI/EI y restauracion de pantalla
   internamente -- el llamante no tiene que preocuparse de eso. --- */
void heap_put_cell(unsigned int idx, const unsigned char bytes[TAM_CELDA]);
void heap_get_cell(unsigned int idx, unsigned char bytes[TAM_CELDA]);

/* Acceso generico de bloque a la pagina de tablas (symtab+strtab). No
   necesitan calculo de pagina/offset como las celdas -- todo cabe en
   una sola pagina fija (PAGINA_TABLAS), asi que el llamante ya trae
   el offset final dentro de esa pagina. */
void tabla_get_bytes(unsigned int offset, unsigned char *destino, unsigned int len);
void tabla_put_bytes(unsigned int offset, const unsigned char *origen, unsigned int len);

/* Pagina una pagina fisica ENTERA en MMU2 y devuelve un puntero al
   inicio de la ventana, SIN restaurar la pantalla ni hacer EI --
   el llamante puede leer/escribir DIRECTAMENTE en esa memoria (es un
   acceso normal, sin funcion de por medio) mientras la pagina siga
   puesta, y debe llamar a heap_page_out() cuando termine.

   Pensado para recorridos SECUENCIALES del heap (como las fases de
   limpiar marcas y barrer del GC) donde procesar una pagina completa
   de 1024 celdas cuesta UN solo cambio de banco, en vez de uno por
   celda (que es lo que hacian heap_get_cell/heap_put_cell, correcto
   pero mucho mas lento para un recorrido secuencial de miles de
   celdas -- medido empiricamente: ~22s para reciclar unas pocas miles
   de celdas antes de este cambio). */
unsigned char *heap_page_in(unsigned int pagina) __z88dk_fastcall;
void heap_page_out(void);

#endif