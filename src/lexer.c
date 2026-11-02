#include "diag_pila.h"
#include <string.h>
#include "lexer.h"

char      Line[MAXLEN + 1];
int       Pos = 1;
TokenType LastType;
char      LastText[STRLEN + 1];
long      LastInt;
#ifndef MYLISP_SIN_FLOTANTES
float     LastFloat;
#endif
long      LastRatNum;
long      LastRatDen;

/* Len cachea strlen(Line), actualizado solo por SetLine(). Antes,
   CurChar() llamaba a strlen(Line) en cada invocacion (y se invoca
   varias veces por token), y SkipSpaces() volvia a calcularlo por su
   cuenta -- para una linea de hasta 500 caracteres, un NextToken()
   normal podia recalcular strlen(Line) decenas de veces. */
static int Len = 0;

/* limite seguro de long en Z88DK (32 bits con signo) */
#define MAXLONG 2147483647L

void SetLine(const char *texto) __z88dk_fastcall {
    if (texto != Line) strcpy(Line, texto);
    Pos = 1;
    Len = (int)strlen(Line);
}

static char CurChar(void) {
    if (Pos > Len) return ' ';
    return Line[Pos - 1];   /* Line[Pos] en Pascal (1-based) == Line[Pos-1] en C */
}

static int IsDigitC(char c) {
    return (c >= '0') && (c <= '9');
}

static int IsLetterC(char c) {
    return ((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'));
}

static void SkipSpaces(void) {
    while (Pos <= Len && Line[Pos - 1] == ' ') Pos++;
    if (Pos <= Len && Line[Pos - 1] == ';') Pos = Len + 1;
}

/* SValInt: convierte [start, end) de Line a long, detectando overflow
   antes de que ocurra (igual que la version Pascal). Devuelve 0 si OK,
   -1 si desbordamiento. Ninguna de las funciones static de este
   fichero se llama a si misma ni forma parte de un ciclo de llamadas
   -- locales static seguros en todas ellas. */
static int SValIntRange(int start, int end, long *result) {
    static int i;
    static int neg;
    static long value;
    static long digit;

    neg = 0;
    value = 0;
    i = start;
    if (i < end && Line[i - 1] == '-') { neg = 1; i++; }

    for (; i < end; i++) {
        digit = Line[i - 1] - '0';
        if (value > (MAXLONG - digit) / 10) {
            return -1;
        }
        value = value * 10 + digit;
    }
    *result = neg ? -value : value;
    return 0;
}

/* SValReal: convierte [start, end) de Line a float. No detecta overflow
   (igual que la version Pascal, que confia en el rango de REAL).

   Protegida con MYLISP_SIN_FLOTANTES para medir el coste real de -lm:
   compilando con -DMYLISP_SIN_FLOTANTES esta funcion (con toda su
   aritmetica de coma flotante) no llega a existir en el objeto. */
#ifndef MYLISP_SIN_FLOTANTES
static void SValRealRange(int start, int end, float *result) {
    static int i;
    static int neg;
    static float intpart, fracpart, fdiv;

    neg = 0;
    intpart = 0.0f;
    fracpart = 0.0f;
    fdiv = 1.0f;
    i = start;
    if (i < end && Line[i - 1] == '-') { neg = 1; i++; }

    while (i < end && IsDigitC(Line[i - 1])) {
        intpart = intpart * 10.0f + (Line[i - 1] - '0');
        i++;
    }
    if (i < end && Line[i - 1] == '.') {
        i++;
        while (i < end && IsDigitC(Line[i - 1])) {
            fracpart = fracpart * 10.0f + (Line[i - 1] - '0');
            fdiv *= 10.0f;
            i++;
        }
    }
    *result = neg ? -(intpart + fracpart / fdiv) : (intpart + fracpart / fdiv);
}
#endif 

void NextToken(void) {
    static int start, fstart;
    static int negative;
    static long numpart, denpart;
    static int errcode;
    static int i, j;

    DIAG_PILA();
    SkipSpaces();

    if (Pos > Len) {
        LastType = TOK_EOF;
        LastText[0] = '\0';
        return;
    }

    if (CurChar() == '(') {
        LastType = TOK_LPAREN;
        strcpy(LastText, "(");
        Pos++;
        return;
    }

    if (CurChar() == ')') {
        LastType = TOK_RPAREN;
        strcpy(LastText, ")");
        Pos++;
        return;
    }

    if (CurChar() == '\'') {
        LastType = TOK_QUOTE;
        strcpy(LastText, "'");
        Pos++;
        return;
    }

    if (CurChar() == '"') {
        Pos++;
        j = 0;
        LastText[0] = '\0';
        while (Pos <= Len && CurChar() != '"') {
            if (j < STRLEN) {
                LastText[j++] = CurChar();
            }
            Pos++;
        }
        LastText[j] = '\0';
        if (CurChar() == '"') {
            Pos++;
        } else {
            LastType = TOK_ERROR;
            strcpy(LastText, "STR?");
            return;
        }
        /* los strings NO se normalizan a mayusculas */
        LastType = TOK_STRING;
        return;
    }

    if (IsLetterC(CurChar())) {
        start = Pos;
        while (Pos <= Len &&
               (IsLetterC(CurChar()) || IsDigitC(CurChar()) ||
                CurChar() == '-' || CurChar() == '?' ||
                CurChar() == '!' || CurChar() == '<')) {
            Pos++;
        }
        j = 0;
        for (i = start; i < Pos && j < SYMLEN; i++) {
            char c = Line[i - 1];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            LastText[j++] = c;
        }
        LastText[j] = '\0';
        LastType = TOK_SYMBOL;
        return;
    }

    if (CurChar() == '+' || CurChar() == '*' || CurChar() == '=') {
        LastType = TOK_SYMBOL;
        LastText[0] = CurChar();
        LastText[1] = '\0';
        Pos++;
        return;
    }

    if (CurChar() == '<' || CurChar() == '>') {
        LastText[0] = CurChar();
        LastText[1] = '\0';
        Pos++;
        if (Pos <= Len && Line[Pos - 1] == '=') {
            LastText[1] = '=';
            LastText[2] = '\0';
            Pos++;
        }
        LastType = TOK_SYMBOL;
        return;
    }

    if (CurChar() == '/') {
        LastType = TOK_SYMBOL;
        strcpy(LastText, "/");
        Pos++;
        return;
    }

    negative = 0;
    if (CurChar() == '-') {
        if (Pos + 1 <= Len && IsDigitC(Line[Pos])) {
            negative = 1;
            Pos++;
        } else {
            LastType = TOK_SYMBOL;
            strcpy(LastText, "-");
            Pos++;
            return;
        }
    }

    if (IsDigitC(CurChar())) {
        start = Pos;
        fstart = negative ? Pos - 1 : Pos;
        while (Pos <= Len && IsDigitC(CurChar())) Pos++;

        errcode = SValIntRange(start, Pos, &numpart);
        if (errcode == -1) {
            LastType = TOK_ERROR;
            strcpy(LastText, "DESBORDAMIENTO");
            return;
        }
        if (negative) numpart = -numpart;

        if (Pos <= Len && CurChar() == '/') {
            Pos++;
            if (!IsDigitC(CurChar())) {
                LastType = TOK_ERROR;
                strcpy(LastText, "RAT?");
                return;
            }
            start = Pos;
            while (Pos <= Len && IsDigitC(CurChar())) Pos++;
            errcode = SValIntRange(start, Pos, &denpart);
            if (errcode == -1) {
                LastType = TOK_ERROR;
                strcpy(LastText, "DESBORDAMIENTO");
                return;
            }
            LastType   = TOK_RAT;
            LastRatNum = numpart;
            LastRatDen = denpart;
            return;
        } else if (Pos <= Len && CurChar() == '.') {
#ifdef MYLISP_SIN_FLOTANTES
            LastType = TOK_ERROR;
            strcpy(LastText, "FLT-OFF");
            return;
#else
            if (Pos + 1 > Len || !IsDigitC(Line[Pos])) {
                LastType = TOK_ERROR;
                strcpy(LastText, "FLT?");
                return;
            }
            Pos++;
            while (Pos <= Len && IsDigitC(CurChar())) Pos++;
            SValRealRange(fstart, Pos, &LastFloat);
            LastType = TOK_FLOAT;
            return;
#endif
        } else {
            LastType = TOK_INT;
            LastInt  = numpart;
            return;
        }
    }

    LastType = TOK_ERROR;
    LastText[0] = CurChar();
    LastText[1] = '\0';
    Pos++;
}