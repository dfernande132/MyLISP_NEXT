#ifndef LEXER_H
#define LEXER_H

#define MAXLEN  500   /* longitud maxima de linea de entrada y buffers */
#define SYMLEN  8     /* longitud maxima de un nombre de simbolo */
#define STRLEN  36    /* longitud maxima de un string */

typedef enum {
    TOK_LPAREN, TOK_RPAREN, TOK_QUOTE, TOK_SYMBOL,
    TOK_INT, TOK_RAT, TOK_FLOAT, TOK_EOF, TOK_ERROR,
    TOK_STRING
} TokenType;

/* --- estado del lexer, equivalente a las variables globales de Pascal --- */
extern char      Line[MAXLEN + 1];
extern int       Pos;              /* indice 1-based, igual que en Pascal */
extern TokenType LastType;
extern char      LastText[STRLEN + 1];
extern long      LastInt;
#ifndef MYLISP_SIN_FLOTANTES
extern float     LastFloat;
#endif
extern long      LastRatNum;
extern long      LastRatDen;

/* Fija Line, reinicia Pos=1 y cachea Len (strlen(Line)) una sola vez --
   sustituye al patron repetido "strcpy(Line,...); Pos=1;" que estaba
   duplicado en repl.c, load.c y los ficheros de test. CurChar() usa el
   Len cacheado en vez de recalcular strlen(Line) en cada llamada (antes
   se recalculaba decenas de veces por cada NextToken()). */
void SetLine(const char *texto) __z88dk_fastcall;

void NextToken(void);

#endif