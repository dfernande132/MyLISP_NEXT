; ============================================================
; Banco de regresion MyLISP
; Cargar con: (LOAD "mdv1_regresion")
; Solo muestra tests que FALLAN.
; Si aparece REGRESION-OK al final, todo es correcto.
; ============================================================

(DEFUN CHECK (NOM RES ESP)
  (IF (EQUAL RES ESP) T
    (PROGN (PRINT NOM) (PRINT RES) NIL)))

; --- Aritmetica basica ---
(CHECK 'suma (+ 1 2) 3)
(CHECK 'resta (- 10 3) 7)
(CHECK 'mult (* 4 5) 20)
(CHECK 'div-exacta (/ 10 2) 5)
(CHECK 'div-rat (/ 1 3) 1/3)
(CHECK 'suma-var (+ 1 2 3 4 5) 15)
(CHECK 'resta-var (- 10 1 2 3) 4)
(CHECK 'negacion (- 5) -5)
(CHECK 'div-var (/ 12 2 3) 2)
(CHECK 'mult-var (* 1 2 3 4 5 6 7 8) 40320)

; --- DIV y MOD ---
(CHECK 'DIV (DIV 10 3) 3)
(CHECK 'MOD (MOD 10 3) 1)
(CHECK 'DIV-neg (DIV -10 3) -3)
(CHECK 'MOD-neg (MOD -10 3) -1)

; --- Comparadores ---
(CHECK 'menor (< 3 5) T)
(CHECK 'mayor (> 5 3) T)
(CHECK 'menor-ig (<= 5 5) T)
(CHECK 'mayor-ig (>= 4 5) NIL)
(CHECK 'igual (= 3 3) T)
(CHECK 'NOT-NIL (NOT NIL) T)
(CHECK 'NOT-T (NOT T) NIL)

; --- AND / OR ---
(CHECK 'AND-TT (AND T T) T)
(CHECK 'AND-TN (AND T NIL) NIL)
(CHECK 'AND-vacio (AND) T)
(CHECK 'OR-NNT (OR NIL NIL T) T)
(CHECK 'OR-vacio (OR) NIL)

; --- Predicados de tipo ---
(CHECK 'NUMBERP-n (NUMBERP 5) T)
(CHECK 'NUMBERP-s (NUMBERP 'X) NIL)
(CHECK 'SYMBOLP-s (SYMBOLP 'X) T)
(CHECK 'SYMBOLP-n (SYMBOLP 5) NIL)
(CHECK 'LISTP-l (LISTP '(1 2)) T)
(CHECK 'LISTP-n (LISTP 5) NIL)
(CHECK 'ATOM-s (ATOM 'X) T)
(CHECK 'ATOM-l (ATOM '(1 2)) NIL)
(CHECK 'NULL-nil (NULL NIL) T)
(CHECK 'NULL-l (NULL '(1)) NIL)

; --- Listas ---
(CHECK 'CAR (CAR '(1 2 3)) 1)
(CHECK 'CDR (CDR '(1 2 3)) '(2 3))
(CHECK 'CONS (CONS 1 '(2 3)) '(1 2 3))
(CHECK 'LIST (LIST 1 2 3) '(1 2 3))
(CHECK 'APPEND (APPEND '(1 2) '(3 4)) '(1 2 3 4))

; --- Igualdad ---
(CHECK 'EQ-sym (EQ 'X 'X) T)
(CHECK 'EQ-lst (EQ '(1 2) '(1 2)) NIL)
(CHECK 'EQUAL-lst (EQUAL '(1 2) '(1 2)) T)
(CHECK 'EQUAL-tipos (EQUAL 3 3.0) NIL)
(CHECK 'igual-tipos (= 3 3.0) T)
(CHECK 'EQUAL-rat (EQUAL 1/2 1/2) T)

; --- Control de flujo ---
(CHECK 'COND (COND (NIL 1) (T 2)) 2)
(CHECK 'PROGN (PROGN 1 2 3) 3)
(CHECK 'LET (LET ((X 5) (Y 10)) (+ X Y)) 15)
(CHECK 'LET-nest (LET ((X 1)) (LET ((Y 2)) (+ X Y))) 3)
(CHECK 'IF-T (IF T 1 2) 1)
(CHECK 'IF-NIL (IF NIL 1 2) 2)
(CHECK 'IF-solo (IF NIL 1) NIL)

; --- EVAL ---
(CHECK 'EVAL (EVAL '(+ 2 3)) 5)

; --- Strings: cubre el bug de LastText STRING[16] ---
(CHECK 'str-16 "1234567890123456" "1234567890123456")
(CHECK 'str-17 "12345678901234567" "12345678901234567")
(DEFINE STR-CAS "CAS: variable libre, no evaluable:")
(CHECK 'str-largo STR-CAS "CAS: variable libre, no evaluable:")
(CHECK 'EQUAL-str (EQUAL "hola" "hola") T)
(CHECK 'EQUAL-str2 (EQUAL "hola" "adios") NIL)

; --- Funciones de usuario ---
(DEFUN CUADRADO (X) (* X X))
(CHECK 'CUADRADO (CUADRADO 7) 49)
(DEFUN FACT (N) (IF (= N 0) 1 (* N (FACT (- N 1)))))
(CHECK 'FACT (FACT 10) 3628800)
(DEFUN MAP (F L)
  (IF (ATOM L) NIL
    (CONS (F (CAR L)) (MAP F (CDR L)))))
(DEFUN DOBLE (X) (+ X X))
(CHECK 'MAP (MAP 'DOBLE '(1 2 3)) '(2 4 6))

; --- Fin ---
(PRINT "REGRESION-OK")
(PRINT 'REGRESION-OK)

