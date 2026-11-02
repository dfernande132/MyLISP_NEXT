#ifndef ENTRADA_MULTILINEA_H
#define ENTRADA_MULTILINEA_H

/* lee una linea "cruda" desde el origen (teclado o fichero) en
   'destino' (hasta 'max' caracteres). Devuelve 1 si hay linea, 0 si
   se acabo la entrada (EOF real), 2 para cancelar toda la expresion. */
typedef int (*LectorLinea)(char *destino, int max);

/* opcional (NULL admitido): 1 si 'linea' es un comando de salida
   (BYE/SALIR/QUIT). Solo se consulta cuando el acumulador esta vacio
   -- igual que en el REPL original, para que "BYE" a mitad de una
   expresion multilinea se trate como texto normal, no como salida. */
typedef int (*EsComandoSalida)(const char *linea);

/* opcional (NULL admitido): se llama antes de cada lectura, indicando
   si se esta continuando una expresion (1) o empezando una nueva (0)
   -- para que el REPL pinte "> " o ".. ". LOAD no necesita anunciar
   nada, pasa NULL. */
typedef void (*AnuncioLectura)(int continuando);

/* opcional (NULL admitido): se llama si una linea (sola, o acumulada)
   supera MAXLEN y se descarta. */
typedef void (*ReportarErrorLongitud)(void);

/* Acumula lineas desde 'leer' hasta que los parentesis balanceen
   (via ContarParentesis), dejando la expresion completa lista para
   parsear (via SetLine + NextToken). Devuelve:
     1  -- hay una expresion completa lista (llamar a NextToken())
     0  -- se acabo la entrada (EOF real)
    -1  -- se pidio salir (solo si 'es_salida' se dispara con el
           acumulador vacio) */
int LeerExpresionCompleta(LectorLinea leer, EsComandoSalida es_salida,
                           AnuncioLectura anunciar,
                           ReportarErrorLongitud reportar_error);

#endif