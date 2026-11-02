#ifndef COLORS_H
#define COLORS_H

#include <stdio.h>
#include <conio.h>

/* textcolor()/textbackground() son las funciones oficiales de conio.h,
   documentadas como compatibles con los modos tilemap con atributos
   (64, 66) -- mas seguras que el codigo de control en bruto (CHR 16/17)
   que solo habiamos verificado en el modo ZX clasico (modo 0). */
#define INK(c)   textcolor(c)
#define PAPER(c) textbackground(c)

/* Indices conio: posiciones de PALETA_TINTAS, no valores RGB.
   Los nombres son historicos: BLANCO es crema y GRIS es lavanda.
   El indice 0 tambien selecciona el fondo de cada estilo; su tono
   lo programa PALETA_FONDO, aunque el nombre historico sea NEGRO. */
#define COLOR_NEGRO    0
#define COLOR_AZUL     1
#define COLOR_VERDE    2
#define COLOR_CYAN     3
#define COLOR_ROJO     4
#define COLOR_MAGENTA  5
#define COLOR_AMARILLO 14
#define COLOR_BLANCO   7
#define COLOR_GRIS     8

/* Paleta Next RRRGGGBB, basada en el editor de Fortran (ED-003).
   Indices conio; 15 reservado al cursor. Crema en 7/14, lavanda en 8. */
#define PALETA_TINTAS { 0x00,0x03,0x7d,0x1f,0xe0,0xe3,0xd0,0xda, \
                        0xb7,0x03,0x7d,0x1f,0xe0,0xe3,0xda,0xff }
#define PALETA_FONDO 0x25 /* azul marino */
#define PALETA_CURSOR_FONDO 0xe0
#define PALETA_BORDE 0xb6 /* gris neutro; paleta ULA, separada del tilemap */
#define INDICE_BORDE_ULA 1
#define ESTILO_CURSOR 15

/* Roles de la interfaz: asignan un indice conio a cada uso.
   Cambiar estos aliases intercambia usos sin modificar tonos RGB. */
#define COLOR_FONDO       COLOR_NEGRO
#define COLOR_TITULO      COLOR_BLANCO
#define COLOR_VERSION     COLOR_VERDE
#define COLOR_COPYRIGHT   COLOR_VERDE
#define COLOR_SEPARADOR   COLOR_ROJO
#define COLOR_INSTRUC     COLOR_VERDE
#define COLOR_PROMPT      COLOR_BLANCO
#define COLOR_ENTRADA     COLOR_AMARILLO
#define COLOR_RESULTADO   COLOR_VERDE
#define COLOR_ERROR       COLOR_ROJO
#define COLOR_INFO        COLOR_GRIS
#define COLOR_GC          COLOR_INFO

/* Los errores lexicos y de evaluacion usan el mismo rojo. */
#define COLOR_ERROR_DESTACADO COLOR_ERROR

#endif
