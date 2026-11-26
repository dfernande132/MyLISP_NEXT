# MyLISP (ZX Spectrum Next) v1.1 — Technical Reference

A LISP interpreter for the ZX Spectrum Next written in C and built with z88dk (`zcc +zxn`, sccz80,
classic library, NEX output), for NextZXOS.

Lineage: the original is **MYLISP.pas**, in Pascal, for the Sinclair QL. This C port for the Next
comes from it, and the MEGA65 port was later derived from the Next code. It is the same language on
another machine, not a variant.

It is designed as a computer-algebra engine: it favours mathematical exactness over convenience,
strict lexical scope, and arithmetic safety over specific types.

> **About this document.** Version 1.1 rewrites, and checks against the source code and against
> tests, the parts that changed since 1.0: limits (§2), the numeric system (§4), memory and GC (§9),
> `LOAD` (§11), depth limits (§13) and code notes (§15). The 1.0 reference claimed things the code did
> not do (a 32,000-cell heap, 200 symbols, 50 strings, a "re-entrant" LOAD, no GC during `LOAD`,
> "complete" rational arithmetic, an overflow error the code never raised) and they are corrected here. The language chapters that did not
> change (§5–§8, §12, §14) are kept from 1.0 with the corrections that could be verified. Where
> something has not been measured, this document says so.
> Error messages are shown as the normal build prints them (English, `src/messages_en.c`);
> `src/messages_es.c` is the Spanish translation and is selected at build time.

---

## 1. General architecture

MyLISP is a classic LISP interpreter with these layers:

- **Lexer** (`lexer.c`): tokenizes character by character. It recognizes integers, rationals,
  floats, symbols, strings and delimiters. Symbols are upper-cased. A rational literal is
  **normalized in the lexer itself** (`4/8` → `1/2`, `6/3` → `2`, `1/0` → `LEX ERR: RAT?`).
- **Parser** (`parser.c`): builds the tree as linked pairs in the heap. It is recursive in C, with a
  depth limit (§13).
- **Evaluator** (`eval_iterativo.c`, `eval.c`): `EvalIterativo` is a single loop with no C
  recursion and an **explicit task stack** in paged memory separate from the heap. Recursion in a
  user's Lisp program does **not** use the Z80's native stack: it only uses tasks (8,192) and heap
  cells. `EvalIterativo` is re-entrant (through `EVAL` and `LOAD`), and those two paths are bounded
  (§13).
- **Arithmetic** (`aritmetica.c`, `racional.c`): exact addition, subtraction, multiplication,
  division and comparators over 32-bit integers and rationals (§4). It lives in page 92.
- **Heap** (`heap_ops.c`, `heap_paged.c`): 32,768 cells of 8 bytes in 32 physical 8 KB pages,
  accessed through an MMU2 window. Cells are addressed by index, not by pointer.
- **Collector** (`gc.c`): mark-sweep with depth-bounded marking (§9, §13).
- **REPL** (`repl.c`, `entrada_multilinea.c`, `teclado_next.c`): read-eval-print loop with a line
  editor, multi-line input and cooperative cancellation (Esc or CAPS SHIFT+SPACE).

The code is **paged**: the Z80 only sees 64 KB, so the resident code (`$8000–$BFFF`) calls modules
that live in physical pages 90–95 mapped at `$C000` (MMU6) through resident gates. The full map is in
the repository README and in §15.

---

## 2. System limits

| limit | value | what happens when reached |
|---|---|---|
| heap cells | **32,768** (8 B each) | `ERR: heap cells exhausted`; the expression is abandoned, GC runs and the prompt returns |
| distinct symbols | **512** | `ERR: symbol table exhausted`; GC does not free symbols, only `(NEW)` does |
| symbol length | **8** | silently truncated |
| simultaneous strings | **90** | `ERR: string table exhausted` |
| string length | **36** | truncated |
| characters per expression | **500** | `ERR: expression too long`, the accumulated input is discarded |
| evaluator task stack | **8,192** | `ERR: evaluation task stack exhausted` |
| integers | ±2,147,483,647 | `ERR: overflow` (§4) |

Symbols are **never freed** during a session: loading several files in a row adds up their symbols.
The GC does reclaim cells and strings. The *builtin* symbols already take entries at startup.

**Only 8 characters are significant, and the truncation is silent.** `CALCULAR-SUMA` and
`CALCULAR-PROD` are the same symbol (`CALCULAR`): the second `DEFUN` shadows the first without any
warning. Make sure the first eight characters of each name are unique.

There is no separate float table: each `TFLOAT` is packed inside its own cell.

After a detected exhaustion the interpreter abandons the expression, runs the GC and returns control
to the prompt with `Expression aborted. GC completed; ready.`. Valid global definitions are kept; the
expression is not retried. During a `LOAD`, exhaustion aborts the load.

---

## 3. Data types

Seven internal cell types:

| Internal type | Description | Literal example |
|---|---|---|
| `TINT` | 32-bit signed integer | `42`, `-7` |
| `TRAT` | Exact rational, always normalized | `1/3`, `-5/2` |
| `TFLOAT` | Native z88dk floating point, 6 bytes packed in the cell | `3.0`, `-2.5` |
| `TSYM` | Symbol | `X`, `MI-FUN?` |
| `TSTRING` | Double-quoted string | `"file.lsp"` |
| `TPAIR` | Pair `(car . cdr)`, the basis of lists | `(1 2 3)` |
| `TCLOSURE` | Function with its captured lexical environment | — |

A `TRAT` takes several cells: the `TRAT` cell, a pair `(num . den)` and the two `TINT` cells for the
numerator and denominator. Rationals are therefore more expensive in heap than integers.

`T` (true) and `NIL` (false / empty list) are special symbols that are always alive. Floats print
with **four decimals** (`(/ 1.0 4)` → `0.2500`).

---

## 4. The numeric system: exact arithmetic

**Principle: MyLISP never promotes an integer to a float on its own.** A CAS that turns `13` into
`13.000001` through a silent overflow produces wrong algebraic results that propagate unnoticed. The
float is an explicit INPUT type, not an automatic output.

> **New in 1.1.** In 1.0 rationals existed as a literal but did not take part in arithmetic:
> `(/ 1 3)` gave `0` (integer division) and overflow was not detected, the result wrapped around in
> 32 bits. Since 1.1 the Next implements the same exact arithmetic as MyLISP65
> (MEGA65), with identical results on both. The code is a port of MEGA65's `racional.c`.

### The three types and their rules

**Integer.** Range ±2,147,483,647 (32-bit signed; −2,147,483,648 is not used, so the range is
symmetric). If an operation would leave it, it gives `ERR: overflow` **before** producing the wrong
number.

**Rational.** Produced by `/` when the division of two exact numbers is not an integer, or by an
`a/b` literal. Always **normalized**: reduced by the GCD, sign in the numerator and **collapsed to an
integer when the denominator becomes 1**. There is one representation per value, which is why
`EQUAL` works for comparing them.

**Float.** Appears only if the literal has a decimal point. As soon as one operand is a float, the
whole operation is a float.

```lisp
(/ 1 3)          ; -> 1/3
(/ 4 6)          ; -> 2/3        reduces
(/ 10 2)         ; -> 5          collapses to integer
4/8              ; -> 1/2        the literal is reduced too
(/ 10 -3)        ; -> -10/3      the sign goes to the numerator
(+ 1/2 1/3)      ; -> 5/6
(+ 1/3 1/3 1/3)  ; -> 1          collapses
(* 3 1/3)        ; -> 1
(+ 1 1/2)        ; -> 3/2        integers and rationals mix freely
(+ 1/2 0.5)      ; -> 1.0000     one float contaminates the whole operation
(+)              ; -> 0
(*)              ; -> 1
```

### `/` versus `DIV`/`MOD`

| | result | type |
|---|---|---|
| `(/ 10 3)` | `10/3` | exact rational |
| `(/ 10 2)` | `5` | integer |
| `(/ 1.0 4)` | `0.2500` | float |
| `(DIV 7 2)` | `3` | integer truncated toward zero |
| `(MOD 7 2)` | `1` | remainder, with the sign of the dividend |

`DIV` and `MOD` accept **integers only**: `(DIV 1/2 2)` gives `ERR: expected integer`. Division by
zero (`(/ 1 0)`, `(/ 1/2 0)`, `(/ 1 0.0)`, `(DIV 7 0)`) gives `ERR: division by zero`.

**`(/ x)` with a single argument returns `x`, not `1/x`.** Inherited behaviour, common to all ports;
noted, not fixed.

### Equality and comparison

| | result | criterion |
|---|---|---|
| `(= 3 3.0)` | `T` | same numeric value, ignores type |
| `(= 1/2 0.5)` | `T` | |
| `(= 1/3 0.3333)` | `NIL` | **exact comparison**: it does not go through floating point |
| `(EQUAL 3 3.0)` | `NIL` | different types |
| `(EQUAL 4/8 2/4)` | `T` | both normalize to `1/2` |
| `(EQUAL (+ 1/2 1/2) 1)` | `T` | the result collapsed to an integer |
| `(EQ 'X 'X)` | `T` | same symbol |

`EQ` compares cell indices (or the same symbol): **it is not for numbers or lists**; two equal
literals are different cells. Use `=` for numbers and `EQUAL` for structures.

The comparators `= < > <= >=` compare **exactly by cross product** (with a sign shortcut), without
going through floating point. If any operand is a float they compare as floats. If the cross product
leaves the range they give `ERR: overflow` rather than silently falling back to floating point. They
check types: `(< 'A 3)` gives `ERR: expected integer` (in 1.0 they read the symbol's internal index as
an integer).

### Overflow

```lisp
(* 100000 100000)          ; -> ERR: overflow
(+ 2147483647 1)           ; -> ERR: overflow
(- -2147483647 1)          ; -> ERR: overflow
(* 46340 46340)            ; -> 2147395600   fits
(* 46341 46341)            ; -> ERR: overflow
(+ 1/100000 1/99999)       ; -> ERR: overflow
(* 1/100000 100000)        ; -> 1            reduces before multiplying, no overflow
(< 2147483647/2 2147483645/3)   ; -> ERR: overflow  (the cross product does not fit)
```

Rationals overflow much sooner than integers, because adding two fractions multiplies denominators.
The implementation **reduces by the GCD before multiplying** precisely to delay that, but the range
is still 32 bits.

---

## 5. Syntax

### Identifiers

Symbols start with a letter and can continue with letters, digits and the characters `-`, `?`, `!`
and `<`. They are converted to upper case (`mi-fun` and `MI-FUN` are the same symbol). Only the first
8 characters are significant.

```lisp
MI-FUNCION   ; valid
PAR?         ; valid (convention: predicates end in ?)
STR<         ; valid (convention: comparators end in <)
3X           ; invalid: cannot start with a digit
```

### Strings

Double-quoted; their main use is passing file names to `LOAD`. At most 36 characters (any excess is
truncated).

### Comments

`;` starts a comment up to the end of the line, in the REPL and in files loaded with `LOAD`.

### Rational literals

```lisp
1/3    ; TRAT
4/8    ; reduced on reading: 1/2
6/3    ; reduced on reading: 2 (integer)
1/0    ; LEX ERR: RAT?
```

---

## 6. Immutability

MyLISP has no `SETQ` and no other way to modify a binding. This is a **design decision**, not a
limitation. Once a symbol is bound (by `DEFINE`, `DEFUN` or a parameter), the binding is permanent in
that environment.

`DEFINE` always adds a new binding to the global environment. If you define the same symbol twice,
the second entry shadows the first in lookups, but the original cell is not modified.

For algorithms with state, the idiomatic solution is to pass accumulators in recursive functions:

```lisp
(DEFUN SUMA-LISTA (L ACC)
  (IF (NULL L) ACC
      (SUMA-LISTA (CDR L) (+ ACC (CAR L)))))

(SUMA-LISTA '(1 2 3 4 5) 0)   ; -> 15
```

---

## 7. Lexical scope and closures

`LAMBDA` captures the lexical environment at the moment it is created:

```lisp
(DEFINE crear-sumador
  (LAMBDA (n)
    (LAMBDA (x) (+ x n))))

(DEFINE suma5 (crear-sumador 5))
(suma5 10)   ; -> 15  (n stays captured as 5)
```

**`DEFUN` is sugar:** `(DEFUN F (X) body)` is equivalent to `(DEFINE F (LAMBDA (X) body))`.

**`LET`:** the binding values are all evaluated in the previous environment, not in each other:

```lisp
(LET ((X 1)
      (Y X))    ; this X is the one from the outer environment, not the 1 just bound
  (+ X Y))
```

**Single namespace (Lisp-1).** Variables and functions share one environment. But **in operator
position, special forms and primitives always win** over any user binding: after `(DEFINE LIST 5)`
the symbol `LIST` evaluated on its own is `5`, but `(LIST 1 2 3)` still calls the primitive. (The 1.0
reference said the opposite; the code resolves the operator with `EsBuiltin` before looking it up in
the environment.)

**Validation in `DEFINE`/`DEFUN`/`LET`/`LAMBDA`.** It is identical to MyLISP65's and is kept that way
by a parity decision between ports: `(DEFINE 5 3)` and `(DEFINE "S" 3)` raise no error and bind the
internal index as if it were a symbol; `(DEFUN T (X) X)` and `(DEFUN + (X) X)` are accepted (the
primitive still wins); non-symbol or repeated parameters are accepted; `(DEFUN F (X))` defines a
function with an empty body. If this is ever tightened, it must be done in all ports at once.

---

## 8. Special forms

They do not evaluate all their arguments in the standard way.

| Form | Evaluation |
|---|---|
| `(QUOTE expr)` or `'expr` | Does not evaluate `expr` |
| `(IF test then)` | Evaluates only `test`; if true evaluates `then`; otherwise returns `NIL` |
| `(IF test then else)` | Evaluates `test` and only the chosen branch |
| `(COND (t1 e1) ...)` | Evaluates tests in order; stops at the first true one |
| `(AND e1 e2 ...)` | Short-circuit; `(AND)` is `T`; returns the last value |
| `(OR e1 e2 ...)` | Short-circuit; `(OR)` is `NIL`; returns the first non-`NIL` |
| `(PROGN e1 e2 ...)` | Evaluates in order; returns the last value |
| `(LET ((v1 e1) ...) body)` | Evaluates the `ei` in the current environment; the body in the new one |
| `(LAMBDA (params) body)` | Creates a closure without evaluating the body |
| `(DEFINE sym val)` | Evaluates `val`; adds a binding to the global environment |
| `(DEFUN f (params) body)` | Sugar for `(DEFINE f (LAMBDA ...))`; returns the name |
| `(LOAD "file")` | Does not evaluate its argument; reads and evaluates the file (§11) |
| `(STATUS)` | Shows heap, symbols, strings and CPU speed. Returns `VOID` |
| `(CLEAN)` | Forces a GC and shows before and after. Returns `VOID` |
| `(NEW)` | Resets the interpreter asking for confirmation; only directly at the prompt |
| `(SYMBOLS)` | Lists the user-defined symbols in the global environment |
| `HELP` / `(HELP)` | Paginated help |

`EVAL` is a **primitive**, not a special form: its argument is evaluated once like any argument (this
yields the expression as data) and the result is evaluated again in the global environment.

---

## 9. Memory management and the garbage collector

### The heap

32,768 cells of 8 bytes (256 KiB) in physical pages 50–81. Each cell holds a tag, a mark and the
payload (two cell indices for a pair, a 32-bit integer, the packed 6-byte float, or a closure's
indices). Alongside it live the string table (90 entries of 36 characters) and the symbol table (512
of 8), together in page 49 (7,938 of 8,192 bytes). The evaluator's task stack is in pages 82–89
(8,192 tasks of 8 bytes). There is no dynamic allocation from the operating system.

### The collector

Mark-sweep. It triggers **only between expressions**, never inside a constructor holding C temporaries:

- in the REPL, when heap usage passes **80 %** or the string table reaches 80 % of its size;
- during a `LOAD`, between expressions, with a threshold of **50 %** (loading a long file leaves
  room for the next expression).

The automatic GC is **silent**. `(CLEAN)` does print `GC before: n%` and `GC after: n%` (plus the
number of strings), because the user types it on purpose.

Roots: `NIL`, `T`, the global environment and **the evaluator's task stack** (including `LET` local
environments and `EVAL`/`LOAD` contexts). That is why `(CLEAN)` is safe in the middle of an
evaluation and also inside a file loaded with `LOAD`. (The 1.0 reference said `(CLEAN)` inside a
`LOAD` was ignored, and the code never did that.)

Marking is recursive in C but **bounded to 32 levels**. At the limit, the child is left unmarked and
noted; afterwards `RemarcarTruncados()` collects it with passes over the heap. It is always correct,
just slower on very deeply nested structures. Long flat lists are walked as a loop along the `cdr`.
Strings are compacted safely.

---

## 10. The REPL and its limits

**Multi-line input.** If parentheses remain open, `..` appears and the continuation is awaited; lines
accumulate until they balance.

**500-character limit** on the accumulated expression, the same in the REPL and in `LOAD`. On
overflow: `ERR: expression too long` and the accumulated text is discarded.

**Cooperative cancellation:** Esc or CAPS SHIFT+SPACE cancel input or request cancelling the
evaluation. A primitive, a print, a GC pass or blocking I/O may finish first; effects already
performed are kept.

**`BYE`** (without parentheses) ends: it shows the farewell and stops in a final loop; it **does not
return to BASIC**, restart with your usual procedure.

---

## 11. The LOAD function

```lisp
(LOAD "cas.lsp")   ; file name in quotes
(LOAD 'MIBIBL)     ; or as a symbol, without quotes
```

The name is passed as-is to esxdos. `LOAD` reads the file line by line, accumulating until the
parentheses balance, and evaluates each complete expression showing the result as the REPL does.

1. **An error in one expression does not stop the load**: it is shown and the next expression
   continues. The only things that abort are resource exhaustion (cells, strings, symbols or tasks)
   and cancellation.
2. **`LOAD` cannot be nested.** A `LOAD` inside a file being loaded gives `ERR: nested LOAD` and is
   not executed; the outer load continues normally. The file reader shares state, which is why. (The
   1.0 reference said it was re-entrant and it was not; in 1.0 it was unguarded, and since 1.1 it is
   rejected.) To chain files, load them one at a time **from the prompt**. A `LOAD` typed at the
   prompt, including inside `EVAL` (§13), works.
3. **`EVAL` does work inside a loaded file**, up to the 5-level limit (§13), which accumulates
   across the `LOAD`: a file launched from inside 5 `EVAL`s cannot use `EVAL` any more.
4. The GC runs between expressions at 50 % (§9) and `(CLEAN)` works inside a file.
5. A missing file gives `ERR: not found: name`.

---

## 12. Quick primitive reference

### List constructors and selectors

| Function | Description |
|---|---|
| `(CAR list)` | First element. Error if `list` is `NIL` or an atom |
| `(CDR list)` | Rest. Error if `list` is `NIL` or an atom |
| `(CONS x list)` | New pair with `x` as `car` and `list` as `cdr` |
| `(LIST e1 e2 ...)` | New list with all the arguments |
| `(APPEND l1 l2)` | Concatenates two lists. `l1` is rebuilt (iteratively); `l2` is shared |

### Predicates

| Function | Description |
|---|---|
| `(ATOM x)` | `T` if `x` is not a pair |
| `(NULL x)` | `T` if `x` is `NIL` |
| `(NOT x)` | `T` if `x` is `NIL`; `NIL` otherwise |
| `(NUMBERP x)` | `T` if `x` is `TINT`, `TRAT` or `TFLOAT` |
| `(SYMBOLP x)` | `T` if `x` is `TSYM` |
| `(LISTP x)` | `T` if `x` is `TPAIR` or `NIL` |
| `(EQ x y)` | `T` if it is the same cell or the same symbol |
| `(EQUAL x y)` | `T` if same structure and same exact type (32-level limit, §13) |

### Arithmetic (§4)

| Function | Description |
|---|---|
| `(+ e1 e2 ...)` | Variadic sum. `(+)` → `0` |
| `(- e1 e2 ...)` | Variadic subtraction. `(- x)` → negation |
| `(* e1 e2 ...)` | Variadic product. `(*)` → `1` |
| `(/ e1 e2 ...)` | Exact division. `TRAT` if not exact. `(/ x)` → `x` |
| `(DIV a b)` | Integer quotient. `TINT` only. Truncates toward zero |
| `(MOD a b)` | Integer remainder. `TINT` only. Sign of the dividend |

### Comparators

| Function | Description |
|---|---|
| `(= a b)` | Numeric value equality (ignores type), exact |
| `(< a b)`, `(> a b)` | Strictly less / greater, exact |
| `(<= a b)`, `(>= a b)` | Less or equal / greater or equal, exact |

### Utilities

| Function | Description |
|---|---|
| `(EVAL expr)` | Evaluates `expr` as code in the global environment (at most 5 nested levels) |
| `(PRINT x)` | Prints `x` (strings with quotes) and a newline. Returns `x`. For debugging |
| `(DISPLAY x)` | Prints `x` without quotes or newline. Returns a special value the REPL does not print |
| `(NEWLINE)` | Emits a newline |
| `(SYMNAME sym)` | Converts a symbol to a `TSTRING` (truncated to 8 characters) |
| `(STR< a b)` | Alphabetical comparison. Accepts `TSTRING` or `TSYM` |
| `(STRCAT a b)` | Concatenates converting to string. Accepts `TSTRING`, `TSYM`, `TINT`, `TRAT`; not `TFLOAT`. Truncates to 36 |

---

## 13. Depth limits and diagnostics

Recursion **in a Lisp program** is not in this table: it lives in the task stack (8,192). But some
of the interpreter's own walks do use the Z80 native stack, and an insufficient stack **corrupts
memory without raising an error**: that is why they have limits, and the limits are not cosmetic.
(1.0 had none of these limits; an expression of 500 characters of parentheses or quotes could
overflow the stack.)

| walk | limit | what happens beyond it |
|---|---|---|
| parser (parentheses and quotes) | **40** | `ERR: nested too deeply`, the expression is discarded |
| nested `EVAL` | **5** | `ERR: EVAL nested too deeply` |
| `EQUAL` | **32** | `ERR: nested too deeply` |
| printing | **64** | deeper levels are printed as `...` |
| GC marking | **32** | rescanned (correct, only slower) |
| `LOAD` inside `LOAD` | **0** | `ERR: nested LOAD` |

`APPEND` is iterative and has no limit. The `EVAL` counter is **global** and accumulates across a
`LOAD`: a `LOAD` made from inside 5 `EVAL`s works, but every `EVAL` that file runs will exceed the
limit.

### Native stack reserve

The Z80 stack lives in MMU7 (`$E000–$FFFF`), growing down from `$FFFF`. In the normal 1.1 build:

- resident code: **15,432 bytes** in `$8000–$BFFF` (952 left);
- fixed constants, data and BSS: **5,438 bytes** from `$E000` (ending at `$F53E`);
- **nominal stack margin: 2,753 bytes** (`$FFFF − $F53E`). This is a link-map figure, not a guarantee
  of free stack during every operation.

Measured in the simulator with the diagnostic build (`diag_pila.*`): `(+ 1 2)` leaves at least 2,285
bytes free; `EVAL` ×5 with rational arithmetic, 2,005; the worst combined case tested, 1,783. These
were measured with z88dk `da80b5fe` and must not be extrapolated to another compiler.

### Diagnostics

`(STATUS)` shows (example values):

```
Heap: 245/32768 (1%)
Symbols: 47/512
Strings: 12/90
CPU ...        (requested and effective speed)
```

The Next does **not** publish the peak depths reached (MyLISP65 does that in `STATUS`).

---

## 14. Programming tips

### Strings versus symbols

| Feature | Symbol | String |
|---|---|---|
| Maximum length | 8 characters | 36 characters |
| Table | `symtab` (512) | `strtab` (90) |
| GC handling | Permanent | Freed when unreferenced |
| Self-evaluates to | Its value in the environment (or error) | The string itself |
| Typical use | Function and variable names | File names for `LOAD` |

Use symbols for labels, test names and internal identifiers; use strings only for text longer than 8
characters or for `LOAD`. A test bench that labels with strings (`(CHECK "test sum" ...)`) exhausts
the table of 90 easily; with symbols (`(CHECK 'test-sum ...)`) it does not, although each new symbol
uses one of the 512 entries.

### `DISPLAY` versus `PRINT`

| | `PRINT` | `DISPLAY` |
|---|---|---|
| Strings | With quotes | Without quotes |
| Newline | Always | Never; use `(NEWLINE)` |
| Return value | The value itself | `VOID` (the REPL does not print it) |
| Typical use | Debugging | End-user output |

### Sorting symbols

```lisp
(DEFUN SYM< (A B) (STR< (SYMNAME A) (SYMNAME B)))
(STR< 'ALFA 'BETA)   ; -> T   (STR< accepts symbols directly, creating no string)
```

### Deep recursion

User recursion does not use the CPU stack. The real limits are the **8,192 tasks** and the 32,768
cells: each call level pushes several tasks, so the maximum number of levels is well below 8,192.
**This document does not measure** how many levels each form of recursion allows; when exhausted the
error is `ERR: evaluation task stack exhausted` or `ERR: heap cells exhausted`, not a hang.

### Session tools

`(STATUS)` (table and CPU state), `(CLEAN)` (immediate GC), `(NEW)` (reset with confirmation, only
from the prompt), `(SYMBOLS)` (user definitions), `HELP`.

---

## 15. Notes for anyone touching the code

**Compiler and linking.** z88dk `v25115-da80b5fe32-20260716`
(`da80b5fe32839b8bd7935ade92e1644f05e8a8f0`), `+zxn`, sccz80, classic library. Build with
`python build.py` or the VS Code task, which use the same source list (`.vscode/tasks.json`). **Do not
compile every `.c` at once**: the wrappers in `src/bancos/` already include other modules and would
produce duplicate definitions.

**Paged code.** Pages 90–95 map at `$C000` (MMU6) and share one window, so **every public function
of a banked module needs a resident gate** in `bancos_codigo.c` (which selects the page, calls
`Real_*` and restores the previous one) and an entry in `tests/interfaces_bancos.json`. Calling
`Real_*` directly bypasses the mapping. Each banked module has a wrapper `src/bancos/banco_*.c` that
sets the segment (`#pragma codeseg`), renames its public functions to `Real_*` and includes the
original `.c`. Current map (code bytes):

| page | contents | bytes |
|---|---|---:|
| 90 | `eval.c` (entry, environments, primitives) | 4,790 |
| 91 | `eval_iterativo.c` | 4,355 |
| 92 | `lexer.c` + `aritmetica.c` (+ `racional.c` included) | 6,819 |
| 93 | banner and help (RODATA) | 2,215 |
| 94 | `gc.c` | 1,303 |
| 95 | `teclado_next.c` | 2,027 |

Each page holds 8,192; page 92 is the fullest (1,373 free). **Resident** code has 952 bytes free:
measure before adding a resident function. After building, `tests/verificar_mapa.py` checks that the
gates exist and are resident, that each `Real_*` is in its page, that no page exceeds 8 KB, that no
resident data sits inside the window, and that the nominal stack reserve is ≥ 2,048 bytes.

**`racional.c` is INCLUDED from `aritmetica.c`; it is not compiled separately** (everything is
`static`). `Aritmetica(args, op)` (with `OP_SUMA/RESTA/MULT/DIV`) and `CompararNumeros(a, b, &cmp)`
are page 92's two gates.

**z88dk BUG: order comparisons on `long` are wrong.** In z88dk `da80b5fe` (classic), `<`, `>`, `<=`
and `>=` on `long` (routine `l_long_cmp`) give an **incorrect** result when the internal subtraction
`a − b` has high byte `0x80` or overflows 32 bits. Measured in the simulator: `0 < 2147483645` fails,
and in 1.0 `(> 2147483647 -1)` returned `NIL`. `==`, `!=`, `+ - * / %` and negation are correct
(verified over about 900 pairs × 3 seeds). So **every order comparison between `long`s goes through
`LongCmp()`/`LongSigno()`**, which compare by 16-bit halves, and the lexer uses them too. New code
that compares large `long`s with `<`/`>` will reproduce the failure. The host (`cc`) behaves
correctly, so **host tests do not detect this bug**; only the Z80N simulator does.

**Re-entrancy and stack.** `EvalIterativo` is re-entrant thanks to `topeInicial`; the entry paths are
the `EVAL` builtin and `DoLoad`. When adding C recursion to any module, bound it with an explicit
limit like those in §13 and measure the stack with the diagnostic build.

**Tests** (in `tests/`, not published):

| test | what it covers |
|---|---|
| `run_etapaA_host.py`, `test_etapaA.c` | depth limits, bounded GC, stack (host, ASan/UBSan) |
| `run_etapaB_host.py`, `test_etapaB.c` | 74 cases + 6,000 random expressions against a 64-bit oracle (no floats: on the host `float` is 4 bytes) |
| `probar_etapaB_z80.py` | the 74 cases on the real NEX in the Z80N simulator (`z88dk-ticks -mz80n`), with 32-bit `long` and floats |
| `probar_pila_z80.py --diag` | 24 stack cases with the diagnostic build |
| `probar_bancos_z80.py`, `verificar_mapa.py` | gates, pages and segments |
| `difftest_z80.py` | runs a `.lsp` form by form in the simulator and compares |
| `lsp_pila/` | chained-`LOAD` battery, for the emulator or hardware (`LEEME.txt`) |

The simulator **does not emulate esxdos**: `LOAD` is tested in the emulator or on hardware.
`REGRESS.LSP` is the common suite of all ports and counts failures honestly; `CAS.LSP` and `PCAS.LSP`
are run by hand after loading it.

What is **not** verified: execution on real hardware, and the exact stack behaviour with a z88dk
version other than the reference one.

---

## Appendix — Error messages (normal build, English)

| message | when |
|---|---|
| `LEX ERR: RAT?` | rational literal with denominator 0 |
| `Expected )` | incomplete expression when input is closed |
| `ERR: expression too long` | more than 500 accumulated characters |
| `ERR: unbound: X` | symbol without a value |
| `ERR: not applicable` | something that is not a function was applied |
| `ERR: wrong args` | wrong number of arguments |
| `ERR: missing argument` | an argument is missing |
| `ERR: expected integer` | non-numeric operand (or non-integer in `DIV`/`MOD`) |
| `ERR: expected list` / `ERR: expected symbol/str` | wrong type |
| `ERR: type not evaluable` | a cell that cannot be evaluated |
| `ERR: division by zero` | zero divisor |
| `ERR: overflow` | outside ±2,147,483,647 (including a comparator's cross product) |
| `ERR: nested too deeply` | parser (40) or `EQUAL` (32) |
| `ERR: EVAL nested too deeply` | more than 5 nested `EVAL`s |
| `ERR: nested LOAD` | `LOAD` inside `LOAD` |
| `ERR: LOAD needs name` | `LOAD` without a valid name |
| `ERR: not found: f` | missing file |
| `ERR: heap cells exhausted` / `symbol table exhausted` / `string table exhausted` / `evaluation task stack exhausted` | resource exhaustion (§2) |
| `ERR: use (NEW) directly at the prompt` | nested `(NEW)` |
