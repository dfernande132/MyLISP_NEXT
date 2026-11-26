#include "diag_pila.h"
#include <stdio.h>
#include "lexer.h"
#include "parser.h"
#include "messages.h"
#include "colors.h"

unsigned int SYM_QUOTE;

void parser_init(void) {
    SYM_QUOTE = InternSym("QUOTE");
}

/* Profundidad actual de anidamiento (parentesis y comillas). Cada
   nivel apila un marco de ParseExpr y otro de ParseList en la pila
   nativa del Z80; sin tope, una expresion de 500 caracteres de
   parentesis o comillas la desbordaria sobre BSS sin aviso. Se
   decrementa en TODAS las salidas, error incluido, asi que queda en 0
   entre expresiones. */
static unsigned char nivelParser;

static unsigned char EntrarNivelParser(void) {
    if (nivelParser >= PARSER_PROFUNDIDAD_MAX) {
        INK(COLOR_ERROR_DESTACADO);
        printf("%s\n", MSG_DEMASIADO_ANIDADO);
        INK(COLOR_ERROR);
        ErrorFlag = 1;
        return 0;
    }
    nivelParser++;
    return 1;
}

CellIdx ParseExpr(void) {
    CellIdx inner, quoted, result;
    DIAG_PILA();

    switch (LastType) {
        case TOK_LPAREN:
            if (!EntrarNivelParser()) return NIL_CELL;
            NextToken();
            inner = ParseList();
            nivelParser--;
            if (LastType == TOK_RPAREN) NextToken();
            return inner;

        case TOK_QUOTE:
            if (!EntrarNivelParser()) return NIL_CELL;
            NextToken();
            quoted = ParseExpr();
            nivelParser--;
            if (HeapError) return NIL_CELL;
            return Cons(MakeSym(SYM_QUOTE), Cons(quoted, NIL_CELL));

        case TOK_SYMBOL:
            result = MakeSym(InternSym(LastText));
            NextToken();
            return result;

        case TOK_INT:
            result = MakeInt(LastInt);
            NextToken();
            return result;

        case TOK_RAT:
            result = MakeRat(LastRatNum, LastRatDen);
            NextToken();
            return result;

#ifndef MYLISP_SIN_FLOTANTES
        case TOK_FLOAT:
            result = MakeFloat(LastFloat);
            NextToken();
            return result;
#endif

        case TOK_STRING:
            result = MakeString(LastText);
            NextToken();
            return result;

        case TOK_ERROR:
            INK(COLOR_ERROR_DESTACADO);
            printf("%s%s\n", MSG_ERROR_LEXICO, LastText);
            INK(COLOR_ERROR);
            ErrorFlag = 1;
            NextToken();   /* avanzar para no quedarse bloqueado en el mismo token */
            return NIL_CELL;

        default:
            return NIL_CELL;
    }
}

/* Iterativa: construye la lista enlazando cada celda nueva al CDR de la
   anterior (SetCdr) en vez de recursar por elemento, de modo que la
   longitud de una lista plana ya no consume pila nativa. La recursion
   que queda es solo por anidamiento, acotada por nivelParser. */
CellIdx ParseList(void) {
    CellIdx cabeza = NIL_CELL, cola = NIL_CELL, first, celda;
    DIAG_PILA();

    for (;;) {
        if (LastType == TOK_RPAREN) break;
        if (LastType == TOK_EOF || ErrorFlag) {
            if (LastType == TOK_EOF && !ErrorFlag) {
                INK(COLOR_ERROR_DESTACADO);
                printf("%s\n", MSG_EXPRESION_INCOMPLETA);
                INK(COLOR_ERROR);
                ErrorFlag = 1;
            }
            break;
        }
        first = ParseExpr();
        celda = Cons(first, NIL_CELL);
        if (HeapError) return NIL_CELL;
        if (cabeza == NIL_CELL) cabeza = celda;
        else SetCdr(cola, celda);
        cola = celda;
    }
    return cabeza;
}

int ContarParentesis(const char *s) __z88dk_fastcall {
    static int count;
    static int en_string;

    count = 0;
    en_string = 0;
    while (*s) {
        char c = *s;
        if (c == ';' && !en_string) break;
        if (c == '"') { en_string = !en_string; s++; continue; }
        if (!en_string) {
            if (c == '(') count++;
            else if (c == ')') count--;
        }
        s++;
    }
    return count;
}