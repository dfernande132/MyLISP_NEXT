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

CellIdx ParseExpr(void) {
    CellIdx inner, quoted, result;
    DIAG_PILA();

    switch (LastType) {
        case TOK_LPAREN:
            NextToken();
            inner = ParseList();
            if (LastType == TOK_RPAREN) NextToken();
            return inner;

        case TOK_QUOTE:
            NextToken();
            quoted = ParseExpr();
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

CellIdx ParseList(void) {
    CellIdx first, rest;
    DIAG_PILA();

    if (LastType == TOK_RPAREN) {
        return NIL_CELL;
    }
    if (LastType == TOK_EOF || ErrorFlag) {
        if (LastType == TOK_EOF && !ErrorFlag) {
            INK(COLOR_ERROR_DESTACADO);
            printf("%s\n", MSG_EXPRESION_INCOMPLETA);
            INK(COLOR_ERROR);
            ErrorFlag = 1;
        }
        return NIL_CELL;
    }
    first = ParseExpr();
    rest  = ParseList();
    return Cons(first, rest);
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