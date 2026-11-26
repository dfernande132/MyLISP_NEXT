# MyLISP (ZX Spectrum Next) v1.1 — Referencia técnica

Intérprete LISP para ZX Spectrum Next escrito en C y compilado con z88dk (`zcc +zxn`, sccz80,
biblioteca *classic*, salida NEX), para NextZXOS.

Genealogía: el original es **MYLISP.pas**, en Pascal, para el Sinclair QL. De ahí salió este port a C
para el Next, y del código del Next salió después el port a MEGA65. Es el mismo lenguaje en otra
máquina, no una variante.

Está pensado como motor de álgebra computacional: prioriza la exactitud matemática sobre la
comodidad, el ámbito léxico estricto y la seguridad aritmética sobre tipos concretos.

> **Sobre este documento.** La versión 1.1 reescribe y verifica contra el código fuente y contra
> pruebas las partes que cambiaron respecto a la 1.0: límites (§2), sistema numérico (§4), memoria y
> GC (§9), `LOAD` (§11), topes de profundidad (§13) y notas de código (§15). La referencia 1.0
> afirmaba cosas que el código no cumplía (heap de 32.000 celdas, 200 símbolos, 50 strings, LOAD
> «reentrante», GC que no corre en `LOAD`, aritmética racional «completa», un error
> de desbordamiento que el código no daba) y se han corregido aquí.
> Los capítulos de lenguaje que no cambian (§5–§8, §12, §14) se conservan de la 1.0 con las
> correcciones que sí se han podido comprobar. Donde algo no está medido, se dice.
> Los mensajes de error se muestran como los imprime la compilación normal (inglés,
> `src/messages_en.c`); `src/messages_es.c` es la traducción y se elige al compilar.

---

## 1. Arquitectura general

MyLISP es un intérprete LISP clásico con estas capas:

- **Lexer** (`lexer.c`): tokeniza carácter a carácter. Reconoce enteros, racionales, flotantes,
  símbolos, strings y delimitadores. Los símbolos se pasan a mayúsculas. Un literal racional se
  **normaliza en el propio lexer** (`4/8` → `1/2`, `6/3` → `2`, `1/0` → `LEX ERR: RAT?`).
- **Parser** (`parser.c`): construye el árbol como pares enlazados en el heap. Es recursivo en C, con
  tope de profundidad (§13).
- **Evaluador** (`eval_iterativo.c`, `eval.c`): `EvalIterativo` es un único bucle sin recursión de C,
  con una **pila explícita de tareas** en memoria paginada aparte del heap. La recursión de un
  programa Lisp del usuario **no** gasta la pila nativa del Z80: solo gasta tareas (8.192) y celdas
  de heap. `EvalIterativo` es reentrante (por `EVAL` y por `LOAD`), y esos dos caminos están
  acotados (§13).
- **Aritmética** (`aritmetica.c`, `racional.c`): suma, resta, producto, división y comparadores
  exactos con enteros y racionales de 32 bits (§4). Vive en la página 92.
- **Heap** (`heap_ops.c`, `heap_paged.c`): 32.768 celdas de 8 bytes en 32 páginas físicas de 8 KB,
  accedidas por una ventana MMU2. Las celdas se direccionan por índice, no por puntero.
- **Recolector** (`gc.c`): mark-sweep con marcado acotado en profundidad (§9, §13).
- **REPL** (`repl.c`, `entrada_multilinea.c`, `teclado_next.c`): bucle de lectura-evaluación-impresión
  con editor de línea, entrada multilínea y cancelación cooperativa (Esc o CAPS SHIFT+SPACE).

El código está **paginado**: el Z80 solo ve 64 KB, así que el código residente (`$8000–$BFFF`)
llama a módulos que viven en páginas físicas 90–95 mapeadas en `$C000` (MMU6) mediante puertas
residentes. El mapa completo está en el README del repositorio y en §15.

---

## 2. Límites del sistema

| límite | valor | qué pasa al llegar |
|---|---|---|
| celdas de heap | **32.768** (8 B cada una) | `ERR: heap cells exhausted`; la expresión se abandona, se ejecuta el GC y vuelve el prompt |
| símbolos distintos | **512** | `ERR: symbol table exhausted`; el GC no libera símbolos, solo `(NEW)` |
| longitud de símbolo | **8** | se trunca **en silencio** |
| strings simultáneos | **90** | `ERR: string table exhausted` |
| longitud de string | **36** | se trunca |
| caracteres por expresión | **500** | `ERR: expression too long`, se descarta lo acumulado |
| pila de tareas del evaluador | **8.192** | `ERR: evaluation task stack exhausted` |
| enteros | ±2.147.483.647 | `ERR: overflow` (§4) |

Los símbolos **no se liberan nunca** durante la sesión: cargar varios ficheros seguidos suma sus
símbolos. El GC sí recupera celdas y strings. Los símbolos *builtin* ya ocupan entradas al arrancar.

**Solo 8 caracteres son significativos, y el recorte es mudo.** `CALCULAR-SUMA` y `CALCULAR-PROD`
son el mismo símbolo (`CALCULAR`): la segunda `DEFUN` sombrea a la primera sin avisar. Conviene que
los primeros ocho caracteres de cada nombre sean únicos.

No hay tabla aparte de flotantes: cada `TFLOAT` va empaquetado en su propia celda.

Tras un agotamiento detectado, el intérprete abandona la expresión, ejecuta el GC y devuelve el
control al prompt con `Expression aborted. GC completed; ready.`. Las definiciones globales
válidas se conservan; la expresión no se reintenta. Durante un `LOAD`, el agotamiento aborta la carga.

---

## 3. Tipos de dato

Siete tipos de celda internos:

| Tipo interno | Descripción | Ejemplo literal |
|---|---|---|
| `TINT` | Entero con signo de 32 bits | `42`, `-7` |
| `TRAT` | Racional exacto, siempre normalizado | `1/3`, `-5/2` |
| `TFLOAT` | Coma flotante nativa de z88dk, 6 bytes empaquetados en la celda | `3.0`, `-2.5` |
| `TSYM` | Símbolo | `X`, `MI-FUN?` |
| `TSTRING` | Cadena entre comillas dobles | `"fichero.lsp"` |
| `TPAIR` | Par `(car . cdr)`, base de las listas | `(1 2 3)` |
| `TCLOSURE` | Función con su entorno léxico capturado | — |

Un `TRAT` ocupa varias celdas: la celda `TRAT`, un par `(num . den)` y las dos celdas `TINT` del
numerador y el denominador. Los racionales son por tanto más caros en heap que los enteros.

`T` (verdad) y `NIL` (falso / lista vacía) son símbolos especiales siempre vivos. Los flotantes se
imprimen con **cuatro decimales** (`(/ 1.0 4)` → `0.2500`).

---

## 4. El sistema numérico: aritmética exacta

**Principio: MyLISP nunca promueve un entero a flotante por su cuenta.** Un CAS que convierte `13`
en `13.000001` por un desbordamiento silencioso produce resultados algebraicos incorrectos que se
propagan sin que nadie lo note. El flotante es un tipo de ENTRADA explícita, no una salida
automática.

> **Novedad de la 1.1.** En la 1.0 los racionales existían como literal pero no participaban en la
> aritmética: `(/ 1 3)` daba `0` (división entera) y el desbordamiento no se detectaba, el resultado
> daba la vuelta en 32 bits. Desde la 1.1 el Next implementa la misma aritmética exacta que
> MyLISP65 (MEGA65); mismos resultados en ambos. El código es un port de `racional.c` de MEGA65.

### Los tres tipos y sus reglas

**Entero.** Rango ±2.147.483.647 (32 bits con signo; −2.147.483.648 no se usa, así el rango es
simétrico). Si una operación se sale, da `ERR: overflow` **antes** de producir el número incorrecto.

**Racional.** Lo genera `/` cuando la división de dos exactos no es entera, o un literal `a/b`.
Siempre **normalizado**: reducido por el MCD, con el signo en el numerador y **colapsado a entero
cuando el denominador queda en 1**. Hay una sola representación de cada valor, y por eso `EQUAL` sirve
para compararlos.

**Flotante.** Solo aparece si el literal lleva punto decimal. En cuanto un operando es flotante, la
operación completa es flotante.

```lisp
(/ 1 3)          ; -> 1/3
(/ 4 6)          ; -> 2/3        reduce
(/ 10 2)         ; -> 5          colapsa a entero
4/8              ; -> 1/2        el literal también se reduce
(/ 10 -3)        ; -> -10/3      el signo va al numerador
(+ 1/2 1/3)      ; -> 5/6
(+ 1/3 1/3 1/3)  ; -> 1          colapsa
(* 3 1/3)        ; -> 1
(+ 1 1/2)        ; -> 3/2        entero y racional se mezclan sin ceremonia
(+ 1/2 0.5)      ; -> 1.0000     un flotante contamina toda la operación
(+)              ; -> 0
(*)              ; -> 1
```

### `/` frente a `DIV`/`MOD`

| | resultado | tipo |
|---|---|---|
| `(/ 10 3)` | `10/3` | racional exacto |
| `(/ 10 2)` | `5` | entero |
| `(/ 1.0 4)` | `0.2500` | flotante |
| `(DIV 7 2)` | `3` | entero truncado hacia cero |
| `(MOD 7 2)` | `1` | resto, con el signo del dividendo |

`DIV` y `MOD` aceptan **solo enteros**: `(DIV 1/2 2)` da `ERR: expected integer`. La división por
cero (`(/ 1 0)`, `(/ 1/2 0)`, `(/ 1 0.0)`, `(DIV 7 0)`) da `ERR: division by zero`.

**`(/ x)` con un solo argumento devuelve `x`, no `1/x`.** Comportamiento heredado del original y
común a todos los ports; queda anotado, no corregido.

### Igualdad y comparación

| | resultado | criterio |
|---|---|---|
| `(= 3 3.0)` | `T` | mismo valor numérico, ignora el tipo |
| `(= 1/2 0.5)` | `T` | |
| `(= 1/3 0.3333)` | `NIL` | **comparación exacta**: no pasa por coma flotante |
| `(EQUAL 3 3.0)` | `NIL` | tipos distintos |
| `(EQUAL 4/8 2/4)` | `T` | los dos normalizan a `1/2` |
| `(EQUAL (+ 1/2 1/2) 1)` | `T` | el resultado colapsó a entero |
| `(EQ 'X 'X)` | `T` | mismo símbolo |

`EQ` compara índices de celda (o el mismo símbolo): **no sirve para números ni listas**, dos
literales iguales son celdas distintas. Para números usa `=`; para estructuras, `EQUAL`.

Los comparadores `= < > <= >=` comparan **exacto por producto cruzado** (con atajo por signo), sin
pasar por flotante. Si algún operando es flotante, comparan en flotante. Si el producto cruzado se
sale de rango dan `ERR: overflow`, en vez de caer a coma flotante en silencio. Validan el tipo:
`(< 'A 3)` da `ERR: expected integer` (en la 1.0 leían el índice interno del símbolo como entero).

### Desbordamiento

```lisp
(* 100000 100000)          ; -> ERR: overflow
(+ 2147483647 1)           ; -> ERR: overflow
(- -2147483647 1)          ; -> ERR: overflow
(* 46340 46340)            ; -> 2147395600   cabe
(* 46341 46341)            ; -> ERR: overflow
(+ 1/100000 1/99999)       ; -> ERR: overflow
(* 1/100000 100000)        ; -> 1            reduce antes de multiplicar, no desborda
(< 2147483647/2 2147483645/3)   ; -> ERR: overflow  (el producto cruzado no cabe)
```

Los racionales desbordan mucho antes que los enteros, porque sumar dos fracciones multiplica
denominadores. La implementación **reduce por el MCD antes de multiplicar** precisamente para
retrasarlo, pero el rango sigue siendo de 32 bits.

---

## 5. Sintaxis

### Identificadores

Los símbolos empiezan por letra y pueden continuar con letras, dígitos y los caracteres `-`, `?`,
`!` y `<`. Se convierten a mayúsculas (`mi-fun` y `MI-FUN` son el mismo símbolo). Solo los primeros
8 caracteres son significativos.

```lisp
MI-FUNCION   ; válido
PAR?         ; válido (convención: predicados terminan en ?)
STR<         ; válido (convención: comparadores terminan en <)
3X           ; inválido: no puede empezar por dígito
```

### Strings

Entre comillas dobles; su uso principal es pasar nombres de fichero a `LOAD`. Máximo 36 caracteres
(el exceso se trunca).

### Comentarios

`;` inicia un comentario hasta el final de la línea, en el REPL y en ficheros cargados con `LOAD`.

### Racionales literales

```lisp
1/3    ; TRAT
4/8    ; se reduce al leerlo: 1/2
6/3    ; se reduce al leerlo: 2 (entero)
1/0    ; LEX ERR: RAT?
```

---

## 6. Inmutabilidad

MyLISP no implementa `SETQ` ni ninguna forma de modificar un *binding*. Es una **decisión de
diseño**, no una limitación. Una vez ligado un símbolo (por `DEFINE`, `DEFUN` o un parámetro), el
binding es permanente en ese entorno.

`DEFINE` siempre añade un binding nuevo en el entorno global. Si defines dos veces el mismo
símbolo, la segunda entrada sombrea a la primera en las búsquedas, pero no modifica la celda original.

Para algoritmos con estado, la solución idiomática es pasar acumuladores en funciones recursivas:

```lisp
(DEFUN SUMA-LISTA (L ACC)
  (IF (NULL L) ACC
      (SUMA-LISTA (CDR L) (+ ACC (CAR L)))))

(SUMA-LISTA '(1 2 3 4 5) 0)   ; -> 15
```

---

## 7. Ámbito léxico y clausuras

`LAMBDA` captura el entorno léxico en el momento de crearse:

```lisp
(DEFINE crear-sumador
  (LAMBDA (n)
    (LAMBDA (x) (+ x n))))

(DEFINE suma5 (crear-sumador 5))
(suma5 10)   ; -> 15  (n queda capturado como 5)
```

**`DEFUN` es azúcar:** `(DEFUN F (X) cuerpo)` equivale a `(DEFINE F (LAMBDA (X) cuerpo))`.

**`LET`:** los valores de los bindings se evalúan todos en el entorno anterior, no entre sí:

```lisp
(LET ((X 1)
      (Y X))    ; esta X es la del entorno exterior, no el 1 recién ligado
  (+ X Y))
```

**Espacio de nombres único (Lisp-1).** Variables y funciones comparten entorno. Pero **en posición
de operador, las formas especiales y las primitivas ganan siempre** a cualquier binding del
usuario: tras `(DEFINE LIST 5)`, el símbolo `LIST` solo evaluado vale `5`, pero `(LIST 1 2 3)` sigue
llamando a la primitiva. (La referencia 1.0 decía lo contrario; el código resuelve el operador con
`EsBuiltin` antes de buscarlo en el entorno.)

**Validación de `DEFINE`/`DEFUN`/`LET`/`LAMBDA`.** Son idénticas a las de MyLISP65, y se mantienen
así por decisión de paridad entre ports: `(DEFINE 5 3)` y `(DEFINE "S" 3)` no dan error y ligan el
índice interno como si fuera un símbolo; `(DEFUN T (X) X)` y `(DEFUN + (X) X)` se aceptan (la
primitiva sigue ganando); parámetros no símbolo o repetidos se aceptan; `(DEFUN F (X))` define una
función con cuerpo vacío. Si se endurece, debe hacerse en todos los ports a la vez.

---

## 8. Formas especiales

No evalúan todos sus argumentos de la forma estándar.

| Forma | Evaluación |
|---|---|
| `(QUOTE expr)` o `'expr` | No evalúa `expr` |
| `(IF test then)` | Evalúa solo `test`; si verdadero evalúa `then`; si no, devuelve `NIL` |
| `(IF test then else)` | Evalúa `test` y solo la rama elegida |
| `(COND (t1 e1) ...)` | Evalúa los tests en orden; para en el primero verdadero |
| `(AND e1 e2 ...)` | Cortocircuito; `(AND)` es `T`; devuelve el último valor |
| `(OR e1 e2 ...)` | Cortocircuito; `(OR)` es `NIL`; devuelve el primer no-`NIL` |
| `(PROGN e1 e2 ...)` | Evalúa en orden; devuelve el valor de la última |
| `(LET ((v1 e1) ...) cuerpo)` | Evalúa los `ei` en el entorno actual; el cuerpo, en el nuevo |
| `(LAMBDA (params) cuerpo)` | Crea una clausura sin evaluar el cuerpo |
| `(DEFINE sym val)` | Evalúa `val`; añade binding en el entorno global |
| `(DEFUN f (params) cuerpo)` | Azúcar de `(DEFINE f (LAMBDA ...))`; devuelve el nombre |
| `(LOAD "fichero")` | No evalúa el argumento; lee y evalúa el fichero (§11) |
| `(STATUS)` | Muestra heap, símbolos, strings y velocidad de CPU. Devuelve `VOID` |
| `(CLEAN)` | Fuerza un GC y muestra el antes y el después. Devuelve `VOID` |
| `(NEW)` | Reinicia el intérprete pidiendo confirmación; solo directamente en el prompt |
| `(SYMBOLS)` | Lista los símbolos definidos por el usuario en el entorno global |
| `HELP` / `(HELP)` | Ayuda paginada |

`EVAL` es una **primitiva**, no una forma especial: su argumento se evalúa una vez como cualquier
argumento (da el dato-expresión) y el resultado se evalúa otra vez en el entorno global.

---

## 9. Gestión de memoria y recolector de basura

### El heap

32.768 celdas de 8 bytes (256 KiB) en las páginas físicas 50–81. Cada celda lleva un tag, una marca
y la carga útil (dos índices de celda para un par, un entero de 32 bits, el flotante empaquetado de
6 bytes, o los índices de una clausura). Junto a él viven la tabla de strings (90 entradas de 36
caracteres) y la de símbolos (512 de 8), juntas en la página 49 (7.938 de 8.192 bytes). La pila de
tareas del evaluador está en las páginas 82–89 (8.192 tareas de 8 bytes). No hay reserva dinámica del
sistema operativo.

### El recolector

Mark-sweep. Se dispara **solo entre expresiones**, nunca dentro de un constructor con temporales de C:

- en el REPL, cuando el uso del heap pasa del **80 %** o la tabla de strings llega al 80 % de la suya;
- durante un `LOAD`, entre expresión y expresión, con umbral del **50 %** (cargar un fichero largo
  deja margen para la expresión siguiente).

El GC automático es **mudo**. `(CLEAN)` sí imprime `GC before: n%` y `GC after: n%` (más el número
de strings), porque lo teclea el usuario a propósito.

Raíces: `NIL`, `T`, el entorno global y **la pila de tareas del evaluador** (incluidos los entornos
locales de `LET` y los contextos de `EVAL`/`LOAD`). Gracias a eso `(CLEAN)` es seguro en medio de
una evaluación y también dentro de un fichero cargado con `LOAD`. (La referencia 1.0 decía que
`(CLEAN)` dentro de un `LOAD` se ignoraba, y el código nunca lo hizo.)

El marcado es recursivo en C pero **acotado a 32 niveles**. Al llegar al tope, el hijo se deja sin
marcar y se anota; después `RemarcarTruncados()` lo recoge con pasadas sobre el heap. Siempre es
correcto; solo se vuelve más lento con estructuras muy anidadas. Las listas planas largas se
recorren como bucle por el `cdr`. Los strings se compactan de forma segura.

---

## 10. El REPL y sus límites

**Entrada multilínea.** Si quedan paréntesis sin cerrar, aparece `..` y se espera la continuación;
las líneas se acumulan hasta que balancean.

**Límite de 500 caracteres** para la expresión acumulada, igual en el REPL y en `LOAD`. Al pasarse:
`ERR: expression too long` y se descarta lo acumulado.

**Cancelación cooperativa:** Esc o CAPS SHIFT+SPACE cancelan la entrada o piden cancelar la
evaluación. Una primitiva, una impresión, una pasada del GC o una E/S bloqueante pueden terminar
antes; los efectos ya hechos se conservan.

**`BYE`** (sin paréntesis) termina: muestra la despedida y se queda en un bucle final; **no vuelve a
BASIC**, hay que reiniciar con el procedimiento habitual.

---

## 11. La función LOAD

```lisp
(LOAD "cas.lsp")   ; nombre entre comillas
(LOAD 'MIBIBL)     ; o como símbolo, sin comillas
```

El nombre se pasa tal cual a esxdos. `LOAD` lee el fichero línea a línea, acumulando hasta que los
paréntesis balancean, y evalúa cada expresión completa mostrando el resultado como el REPL.

1. **Un error en una expresión no detiene la carga**: se muestra y se sigue con la siguiente. Lo único
   que aborta es el agotamiento de recursos (celdas, strings, símbolos o tareas) y la cancelación.
2. **`LOAD` NO es anidable.** Un `LOAD` dentro de un fichero que se está cargando da
   `ERR: nested LOAD` y no se ejecuta; la carga exterior sigue con normalidad. El lector de ficheros
   comparte estado, por eso. (La referencia 1.0 decía que era reentrante y no lo era; en la 1.0 no
   estaba protegido y desde la 1.1 se rechaza.) Para encadenar ficheros, cárgalos de uno en uno
   **desde el prompt**. Un `LOAD` tecleado en el prompt, también dentro de `EVAL` (§13), sí funciona.
3. **`EVAL` sí funciona dentro de un fichero cargado**, con el tope de 5 niveles (§13), que se
   acumula a través del `LOAD`: un fichero lanzado desde dentro de 5 `EVAL` no puede usar ya `EVAL`.
4. El GC corre entre expresiones al 50 % (§9) y `(CLEAN)` funciona dentro de un fichero.
5. Un fichero inexistente da `ERR: not found: nombre`.

---

## 12. Referencia rápida de primitivas

### Constructores y selectores de lista

| Función | Descripción |
|---|---|
| `(CAR lista)` | Primer elemento. Error si `lista` es `NIL` o un átomo |
| `(CDR lista)` | Resto. Error si `lista` es `NIL` o un átomo |
| `(CONS x lista)` | Nuevo par con `x` como `car` y `lista` como `cdr` |
| `(LIST e1 e2 ...)` | Nueva lista con todos los argumentos |
| `(APPEND l1 l2)` | Concatena dos listas. `l1` se reconstruye (de forma iterativa); `l2` se comparte |

### Predicados

| Función | Descripción |
|---|---|
| `(ATOM x)` | `T` si `x` no es un par |
| `(NULL x)` | `T` si `x` es `NIL` |
| `(NOT x)` | `T` si `x` es `NIL`; `NIL` en otro caso |
| `(NUMBERP x)` | `T` si `x` es `TINT`, `TRAT` o `TFLOAT` |
| `(SYMBOLP x)` | `T` si `x` es `TSYM` |
| `(LISTP x)` | `T` si `x` es `TPAIR` o `NIL` |
| `(EQ x y)` | `T` si es la misma celda o el mismo símbolo |
| `(EQUAL x y)` | `T` si misma estructura y mismo tipo exacto (tope de 32 niveles, §13) |

### Aritmética (§4)

| Función | Descripción |
|---|---|
| `(+ e1 e2 ...)` | Suma variádica. `(+)` → `0` |
| `(- e1 e2 ...)` | Resta variádica. `(- x)` → negación |
| `(* e1 e2 ...)` | Producto variádico. `(*)` → `1` |
| `(/ e1 e2 ...)` | División exacta. `TRAT` si no hay resto. `(/ x)` → `x` |
| `(DIV a b)` | Cociente entero. Solo `TINT`. Trunca hacia cero |
| `(MOD a b)` | Resto entero. Solo `TINT`. Signo del dividendo |

### Comparadores

| Función | Descripción |
|---|---|
| `(= a b)` | Igualdad de valor numérico (ignora el tipo), exacta |
| `(< a b)`, `(> a b)` | Menor / mayor estrictos, exactos |
| `(<= a b)`, `(>= a b)` | Menor o igual / mayor o igual, exactos |

### Utilidades

| Función | Descripción |
|---|---|
| `(EVAL expr)` | Evalúa `expr` como código en el entorno global (máx. 5 niveles anidados) |
| `(PRINT x)` | Imprime `x` (strings con comillas) y salto de línea. Devuelve `x`. Para depuración |
| `(DISPLAY x)` | Imprime `x` sin comillas ni salto de línea. Devuelve un valor especial que el REPL no imprime |
| `(NEWLINE)` | Emite un salto de línea |
| `(SYMNAME sym)` | Convierte un símbolo en `TSTRING` (truncado a 8 caracteres) |
| `(STR< a b)` | Comparación alfabética. Acepta `TSTRING` o `TSYM` |
| `(STRCAT a b)` | Concatena convirtiendo a string. Acepta `TSTRING`, `TSYM`, `TINT`, `TRAT`; no `TFLOAT`. Trunca a 36 |

---

## 13. Topes de profundidad y diagnóstico

La recursión **de un programa Lisp** no está en esta tabla: vive en la pila de tareas (8.192). Pero
hay recorridos del propio intérprete que sí usan la pila nativa del Z80, y una pila insuficiente
**corrompe memoria sin dar error**: por eso tienen tope y no es cosmética. (La 1.0 no tenía estos
topes; con una expresión de 500 caracteres de paréntesis o comillas se podía desbordar la pila.)

| recorrido | tope | qué pasa al superarlo |
|---|---|---|
| parser (paréntesis y comillas) | **40** | `ERR: nested too deeply`, la expresión se descarta |
| `EVAL` anidado | **5** | `ERR: EVAL nested too deeply` |
| `EQUAL` | **32** | `ERR: nested too deeply` |
| impresión | **64** | los niveles más profundos se imprimen como `...` |
| marcado del GC | **32** | se rescanea (correcto, solo más lento) |
| `LOAD` dentro de `LOAD` | **0** | `ERR: nested LOAD` |

`APPEND` es iterativo y no tiene tope. El contador de `EVAL` es **global** y se acumula a través de
un `LOAD`: un `LOAD` hecho desde dentro de 5 `EVAL` funciona, pero cada `EVAL` que ejecute ese
fichero superará el tope.

### Reserva de pila nativa

La pila del Z80 vive en MMU7 (`$E000–$FFFF`), bajando desde `$FFFF`. En la compilación normal 1.1:

- código residente: **15.432 bytes** en `$8000–$BFFF` (quedan 952 libres);
- constantes, datos y BSS fijos: **5.438 bytes** desde `$E000` (acaban en `$F53E`);
- **margen nominal de pila: 2.753 bytes** (`$FFFF − $F53E`). Es una cifra del mapa de enlace, no una
  garantía de pila libre en cada operación.

Medido en el simulador con la compilación de diagnóstico (`diag_pila.*`): `(+ 1 2)` deja como mínimo
2.285 bytes libres; `EVAL` ×5 con aritmética racional, 2.005; el peor caso combinado probado, 1.783. Son medidas con
z88dk `da80b5fe` y no se extrapolan a otro compilador.

### Diagnóstico

`(STATUS)` muestra (valores de ejemplo):

```
Heap: 245/32768 (1%)
Symbols: 47/512
Strings: 12/90
CPU ...        (velocidad solicitada y efectiva)
```

El Next **no** publica los máximos de profundidad alcanzados (eso lo hace MyLISP65 en `STATUS`).

---

## 14. Consejos de programación

### Strings frente a símbolos

| Característica | Símbolo | String |
|---|---|---|
| Longitud máxima | 8 caracteres | 36 caracteres |
| Tabla | `symtab` (512) | `strtab` (90) |
| Gestión por el GC | Permanente | Se libera cuando no hay referencias |
| Autoevalúa a | Su valor en el entorno (o error) | El propio string |
| Uso típico | Nombres de funciones y variables | Nombres de fichero para `LOAD` |

Usa símbolos para etiquetas, nombres de test e identificadores internos; usa strings solo para
texto de más de 8 caracteres o para `LOAD`. Un banco de pruebas que etiquete con strings
(`(CHECK "test suma" ...)`) agota la tabla de 90 con facilidad; con símbolos
(`(CHECK 'test-suma ...)`) no, aunque cada símbolo nuevo consume una de las 512 entradas.

### `DISPLAY` frente a `PRINT`

| | `PRINT` | `DISPLAY` |
|---|---|---|
| Strings | Con comillas | Sin comillas |
| Salto de línea | Siempre | Nunca; usa `(NEWLINE)` |
| Valor de retorno | El propio valor | `VOID` (el REPL no lo imprime) |
| Uso típico | Depuración | Salida para el usuario |

### Ordenar símbolos

```lisp
(DEFUN SYM< (A B) (STR< (SYMNAME A) (SYMNAME B)))
(STR< 'ALFA 'BETA)   ; -> T   (STR< acepta símbolos directamente, sin crear un string)
```

### Recursión profunda

La recursión de usuario no gasta la pila de la CPU. El límite real son las **8.192 tareas** y las
32.768 celdas: cada nivel de llamada apila varias tareas, por lo que el máximo de niveles es bastante
menor que 8.192. **No está medido en este documento** cuántos niveles admite cada forma de recursión;
al agotarse, el error es `ERR: evaluation task stack exhausted` o `ERR: heap cells exhausted`, no un
cuelgue.

### Herramientas de sesión

`(STATUS)` (estado de tablas y CPU), `(CLEAN)` (GC inmediato), `(NEW)` (reinicio con confirmación,
solo desde el prompt), `(SYMBOLS)` (definiciones del usuario), `HELP`.

---

## 15. Notas para quien toque el código

**Compilador y enlace.** z88dk `v25115-da80b5fe32-20260716` (`da80b5fe32839b8bd7935ade92e1644f05e8a8f0`),
`+zxn`, sccz80, biblioteca classic. Se compila con `python build.py` o la tarea de VS Code, que usan
la misma lista de fuentes (`.vscode/tasks.json`). **No compiles todos los `.c` a la vez**: los
envoltorios de `src/bancos/` ya incluyen otros módulos y darían definiciones duplicadas.

**Código paginado.** Las páginas 90–95 se mapean en `$C000` (MMU6) y comparten ventana, así que
**toda función pública de un módulo bancado necesita una puerta residente** en `bancos_codigo.c`
(que selecciona la página, llama a `Real_*` y restaura la anterior) y una entrada en
`tests/interfaces_bancos.json`. Llamar a `Real_*` directamente se salta el mapeo. Cada módulo bancado
tiene un envoltorio `src/bancos/banco_*.c` que fija el segmento (`#pragma codeseg`), renombra sus
funciones públicas a `Real_*` e incluye el `.c` original. Mapa actual (bytes de código):

| página | contenido | bytes |
|---|---|---:|
| 90 | `eval.c` (entrada, entornos, primitivas) | 4.790 |
| 91 | `eval_iterativo.c` | 4.355 |
| 92 | `lexer.c` + `aritmetica.c` (+ `racional.c` incluido) | 6.819 |
| 93 | banner y ayuda (RODATA) | 2.215 |
| 94 | `gc.c` | 1.303 |
| 95 | `teclado_next.c` | 2.027 |

Cada página admite 8.192; la 92 es la más llena (1.373 libres). El código **residente** tiene 952
bytes libres: antes de añadir una función residente, mide. `tests/verificar_mapa.py` comprueba tras
compilar que las puertas existen y son residentes, que cada `Real_*` está en su página, que ninguna
página pasa de 8 KB, que no hay datos residentes dentro de la ventana y que la reserva nominal de
pila es ≥ 2.048 bytes.

**`racional.c` se INCLUYE desde `aritmetica.c`; no se compila aparte** (todo es `static`).
`Aritmetica(args, op)` (con `OP_SUMA/RESTA/MULT/DIV`) y `CompararNumeros(a, b, &cmp)` son las dos
puertas de la página 92.

**BUG DE z88dk: las comparaciones de orden de `long` están mal.** En z88dk `da80b5fe` (classic),
`<`, `>`, `<=` y `>=` sobre `long` (rutina `l_long_cmp`) dan un resultado **incorrecto** cuando la
resta interna `a − b` tiene el byte alto `0x80` o desborda 32 bits. Medido en el simulador:
`0 < 2147483645` falla, y en la 1.0 `(> 2147483647 -1)` devolvía `NIL`. Son correctos `==`, `!=`,
`+ - * / %` y el negado (verificado con unos 900 pares × 3 semillas). Por eso **toda comparación de
orden entre `long` pasa por `LongCmp()`/`LongSigno()`**, que comparan por mitades de 16 bits, y el
lexer lo usa también. Código nuevo que compare `long` grandes con `<`/`>` reproducirá el fallo. No
afecta al host (`cc`), que se comporta bien: **los tests de host no detectan este bug**, solo el
simulador Z80N.

**Reentrada y pila.** `EvalIterativo` es reentrante gracias a `topeInicial`; los caminos de entrada
son el builtin `EVAL` y `DoLoad`. Al añadir recursión de C a cualquier módulo, acótala con un tope
explícito como los de §13 y mide la pila con la compilación de diagnóstico.

**Tests** (en `tests/`, no se publican):

| prueba | qué cubre |
|---|---|
| `run_etapaA_host.py`, `test_etapaA.c` | topes de profundidad, GC acotado, pila (host, ASan/UBSan) |
| `run_etapaB_host.py`, `test_etapaB.c` | 74 casos + 6.000 expresiones aleatorias contra un oráculo de 64 bits (sin flotantes: en el host `float` mide 4 bytes) |
| `probar_etapaB_z80.py` | los 74 casos sobre el NEX real en el simulador Z80N (`z88dk-ticks -mz80n`), con `long` de 32 bits y flotantes |
| `probar_pila_z80.py --diag` | 24 casos de pila con la compilación de diagnóstico |
| `probar_bancos_z80.py`, `verificar_mapa.py` | puertas, páginas y segmentos |
| `difftest_z80.py` | ejecuta un `.lsp` forma a forma en el simulador y compara |
| `lsp_pila/` | batería de `LOAD` encadenado, para el emulador o el hardware (`LEEME.txt`) |

El simulador **no emula esxdos**: `LOAD` se prueba en el emulador o en hardware. `REGRESS.LSP` es
la suite común de todos los ports y cuenta fallos de verdad; `CAS.LSP` y `PCAS.LSP` se pasan a
mano tras cargarla.

Lo que **no** está verificado: ejecución en hardware real, y el comportamiento exacto de pila con una
versión de z88dk distinta de la de referencia.

---

## Apéndice — Mensajes de error (compilación normal, inglés)

| mensaje | cuándo |
|---|---|
| `LEX ERR: RAT?` | literal racional con denominador 0 |
| `Expected )` | expresión incompleta al cerrar la entrada |
| `ERR: expression too long` | más de 500 caracteres acumulados |
| `ERR: unbound: X` | símbolo sin valor |
| `ERR: not applicable` | se aplicó algo que no es función |
| `ERR: wrong args` | número de argumentos incorrecto |
| `ERR: missing argument` | falta un argumento |
| `ERR: expected integer` | operando no numérico (o no entero en `DIV`/`MOD`) |
| `ERR: expected list` / `ERR: expected symbol/str` | tipo incorrecto |
| `ERR: type not evaluable` | celda que no se puede evaluar |
| `ERR: division by zero` | divisor cero |
| `ERR: overflow` | fuera de ±2.147.483.647 (incluido el producto cruzado de un comparador) |
| `ERR: nested too deeply` | parser (40) o `EQUAL` (32) |
| `ERR: EVAL nested too deeply` | más de 5 `EVAL` anidados |
| `ERR: nested LOAD` | `LOAD` dentro de `LOAD` |
| `ERR: LOAD needs name` | `LOAD` sin nombre válido |
| `ERR: not found: f` | fichero inexistente |
| `ERR: heap cells exhausted` / `symbol table exhausted` / `string table exhausted` / `evaluation task stack exhausted` | agotamiento de recursos (§2) |
| `ERR: use (NEW) directly at the prompt` | `(NEW)` anidado |
