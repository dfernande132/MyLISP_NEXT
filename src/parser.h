#ifndef PARSER_H
#define PARSER_H

#include "heap_ops.h"

/* simbolos especiales, interned una vez por parser_init() */
extern unsigned int SYM_QUOTE;

void parser_init(void);

/* ParseExpr consume el token actual (LastType/LastText/...) y construye
   la celda correspondiente, dejando el lexer posicionado en el token
   siguiente al terminar -- igual que en MYLISP.pas. Debe llamarse
   NextToken() una vez antes de la primera llamada. */
CellIdx ParseExpr(void);

/* ParseList consume tokens hasta encontrar TOK_RPAREN o TOK_EOF,
   construyendo la lista de pares correspondiente. Asume que el LPAREN
   inicial ya fue consumido por el llamante. */
CellIdx ParseList(void);

/* cuenta '(' menos ')' en una linea, ignorando lo que haya dentro de
   strings y comentarios -- usado tanto por el REPL como por LOAD para
   saber cuando una expresion multilinea esta completa */
int ContarParentesis(const char *s) __z88dk_fastcall;

#endif