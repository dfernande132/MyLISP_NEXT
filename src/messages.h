#ifndef MESSAGES_H
#define MESSAGES_H

/* --- banner --- */
extern const char *MSG_TITULO;
extern const char *MSG_VERSION;
extern const char *MSG_COPYRIGHT;
extern const char *MSG_ESCRIBE_SALIR;

/* --- prompts --- */
extern const char *MSG_PROMPT_REPL;
extern const char *MSG_PROMPT_CONTINUA;

/* --- errores --- */
extern const char *MSG_ERROR_LEXICO;
extern const char *MSG_EXPRESION_INCOMPLETA;
extern const char *MSG_EXPRESION_LARGA;
extern const char *MSG_HEAP_AGOTADO;
extern const char *MSG_FICHERO_NO_ENCONTRADO;
extern const char *MSG_CARGANDO;
extern const char *MSG_CARGA_COMPLETADA;

extern const char *MSG_SIMBOLO_NO_LIGADO;
extern const char *MSG_NO_APLICABLE;
extern const char *MSG_ARGUMENTOS_INCORRECTOS;
extern const char *MSG_REQUIERE_ARGUMENTOS;
extern const char *MSG_DIVISION_CERO;
extern const char *MSG_TIPO_NO_EVALUABLE;
extern const char *MSG_ESPERABA_ENTERO;
extern const char *MSG_LOAD_ESPERA_STRING;
extern const char *MSG_REQUIERE_LISTA;
extern const char *MSG_ESPERABA_SIMBOLO;

/* --- Grupo G: sistema (STATUS/CLEAN/NEW/SYMBOLS) --- */
extern const char *MSG_STATUS_HEAP;
extern const char *MSG_STATUS_SIMBOLOS;
extern const char *MSG_STATUS_STRINGS;
extern const char *MSG_CLEAN_ANTES;
extern const char *MSG_CLEAN_DESPUES;
extern const char *MSG_NEW_CONFIRMAR;
extern const char *MSG_NEW_REINICIANDO;
extern const char *MSG_NEW_CANCELADO;

/* --- informativos --- */
extern const char *MSG_HASTA_PRONTO;

#endif