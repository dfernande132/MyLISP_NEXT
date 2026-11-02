/* Enlaza codigo en su pagina; RODATA/DATA/BSS siguen residentes. */
#pragma codeseg CODE_PAGE_92
#define SetLine Real_SetLine
#define NextToken Real_NextToken
#include "../lexer.c"
