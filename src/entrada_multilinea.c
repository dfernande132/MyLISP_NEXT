#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "entrada_multilinea.h"

/* Line es el acumulador Y la entrada del lexer: el parser termina
   antes de empezar otra lectura, asi que no hacen falta dos copias
   de MAXLEN+1 bytes. La linea fisica conserva su buffer separado.
   Tras SetLine(Line) no borrar el acumulador: el lexer aun lo necesita.
   La siguiente llamada lo reinicia, despues de consumir la expresion. */
static char entrada_generica[MAXLEN + 1];

int LeerExpresionCompleta(LectorLinea leer, EsComandoSalida es_salida,
                           AnuncioLectura anunciar,
                           ReportarErrorLongitud reportar_error) {
    static int abierto;
    int lectura;

    Line[0] = '\0';
    abierto = 0;

    for (;;) {
        if (anunciar) anunciar(Line[0] != '\0');

        lectura = leer(entrada_generica, MAXLEN);
        if (lectura == 2) {
            Line[0] = 0; abierto = 0;
            continue; /* Esc cancela TODA la expresion, incluso multilinea. */
        }
        if (!lectura) {
            return 0;   /* EOF real */
        }

        if (Line[0] == '\0' && es_salida && es_salida(entrada_generica)) {
            return -1;
        }

        if (Line[0] == '\0') {
            if ((int)strlen(entrada_generica) > MAXLEN) {
                if (reportar_error) reportar_error();
                continue;
            }
            strcpy(Line, entrada_generica);
        } else {
            if ((int)strlen(Line) + 1 + (int)strlen(entrada_generica) > MAXLEN) {
                if (reportar_error) reportar_error();
                Line[0] = '\0';
                abierto = 0;
                continue;
            }
            strcat(Line, " ");
            strcat(Line, entrada_generica);
        }

        abierto += ContarParentesis(entrada_generica);

        if (abierto <= 0) {
            abierto = 0;
            if (Line[0] != '\0') {
                SetLine(Line);
                return 1;
            }
            Line[0] = '\0';
        }
    }
}