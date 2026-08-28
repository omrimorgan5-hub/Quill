# Quill
---

## Why Quill?

- **Built for Math & Numerics** — First-class `number`, `double`, and `char` types, clean arithmetic, and a growing math-oriented standard library. Ideal for algorithms, calculators, simulations, and performance-sensitive numeric code.
- **Blazing Fast** — Compiles to native machine code for near-C performance. The current pipeline transpiles to C and builds with gcc; the long-term backend is **LLVM** for direct, optimized native code generation.
- **Statically Typed** — Type errors are caught at compile time before execution.
- **Simple & Clean Syntax** — Readable syntax with optional semicolons and zero clutter. Spend time on the math, not the boilerplate.
- **Cross-Platform** — Native executable support for **Linux** and **Windows**.
- **Escape Hatch to C** — Call C library functions directly with `extern`, or drop into raw C with `C_call` / `C_top` when you need something Quill doesn't have natively yet.

Modules and the `qpm` package manager are supported, but Quill's primary focus is **fast numeric code** and a clear path to high-performance native compilation — not a large packaging ecosystem.

---

## Installation

Download the pre-compiled binary for your platform from the [Releases](https://github.com/omo5454/Quill/releases) page.

### Windows
Install the quill-windows[32 or 64].exe and run it in powershell.

### Linux
Make the binary executable and run:
```bash
chmod +x quill-linux
./quill-linux myfile.qsc
```

> **Note on macOS:** Support for macOS is currently paused to ensure maximum stability on core Linux and Windows platforms.

---

## Usage

```bash
# Transpile to C and compile to a native binary
quill-[os] --compile myfile.qsc

# Transpile only (produces output.c, does not compile it)
quill-[os] myfile.qsc -o output.c
```

---

## Language Tour

### Comments

`#` runs to the end of the line. There's no block-comment syntax.

```quill
# this is a comment
let x: number = 5; # so is this
```

### Variables

Variables are declared with `let`, `mut`, or `const`. Type annotations are supported.

```quill
let x: number = 5;
let name: string = "Alice";
let pi: double = 3.14;
let active: bool = true;
let ch: char = 'A';
```

> **Note:** `let`, `mut`, and `const` are currently interchangeable — Quill does not yet enforce immutability. A `const` can be reassigned without error. Treat the keyword as documentation of intent for now, not a guarantee.

### Printing

```quill
say "Hello, world!";
printf("Hello " + name);
```

Any type can be concatenated onto a string with `+` — Quill converts it automatically.

### Arithmetic & Incrementing

```quill
let sum: number = 10 + 5;
let product: number = 6 * 7;

let i: number = 0;
i++;   # increment
i--;   # decrement
```

`++`/`--` are statements, not expressions — `arr[i++]` isn't valid; increment on its own line instead.

### Control Flow

Conditions do not require parentheses.

```quill
let score: number = 75;

if score >= 90 {
    say "Grade: A";
} else if score >= 75 {
    say "Grade: B";
} else {
    say "Grade: F";
}

let x: number = 0;
while x < 5 {
    printf("x: " + x);
    x++;
}
```

> **Note:** `elif` is reserved but not yet wired into the parser — use `else if` (two words) for else-if chains.

### Arrays

Fixed-size, declared with a literal size, local to the function they're declared in (not usable as a parameter or return type yet).

```quill
let stack: number[200];
stack[0] = 42;
say stack[0];
```

`number[]`, `double[]`, `string[]`, and `bool[]` all work the same way.

### Functions

Functions must be declared before they are called. The return type annotation is optional — Quill infers it from your `return` statements if you omit it. Two functions can't share a name anywhere in the whole program, including across imports.

```quill
func add(a: number, b: number): number {
    return a + b;
}

func greet(name: string): string {
    return "Hello, " + name;
}

let result: number = add(3, 4);
say greet("Alice");
```

### Standard Library

Quill's own stdlib is intentionally small and math-leaning:

```quill
len("hello")        # -> 5
toString(42)         # -> "42"
input(someNumberVar) # reads a number from stdin into an existing variable
input(someCharVar)   # reads a character (skips leading whitespace)
input(someStringVar) # reads a full line into a string
```

`s[i]` indexes a string, returning that character's code as a `number` (the same way C represents `char`).

Math helpers live under `stdlib/math/` (sqrt, pow, floor, factorial, fib, …). Import them when you need them.

**Bonus:** every generated file always includes C's `<stdio.h>`, `<string.h>`, `<ctype.h>`, and `<stdlib.h>` — so functions like `strcmp`, `strlen`, `isdigit`, `toupper`, `atoi`, and `rand` are callable **by name** once declared with `extern`, with no extra includes required.

### Calling C Directly

For anything else, `C_call("...")` splices raw C directly into your program, at the exact spot it's written:

```quill
C_call("printf(\"%s\\n\", \"raw C, verbatim\");");
```

`C_top("...")` does the same thing, but always lands at file scope, *above* `main()` — use it for extra `#include`s, global variables, and struct definitions when needed.

**Important:** any backslash you want to survive into the generated C must be doubled. Quill's own string lexer un-escapes `\n` into a real newline character *before* `C_call` ever sees it — write `\\n` to get a literal `\n` in the output.

### `extern` Functions

Tell Quill about a C function's signature (implemented by a library you've linked, or already available via the default headers) so it's typed correctly everywhere you use it — without emitting any C definition of your own:

```quill
extern func sqrt(x: double): double;

say sqrt(64);              # correctly formatted as a double
say "root: " + sqrt(64);
```

Skipping this for a `double`/`bool`-returning C function will silently default to treating it as a `number`, which gcc will flag as a format mismatch.

### Modules & Imports

`import "path.qsc";` splices another file's entire contents — functions *and* top-level code — in, exactly where the `import` line sits (much like C's `#include`). Relative paths resolve against the importing file's own directory. Circular imports are caught with a clear error.

```quill
# mathlib.qsc
func square(x: number): number {
    return x * x;
}

# main.qsc
import "mathlib.qsc";
say square(5);
```

**Installing shared modules from GitHub:** use `qpm`, Quill's package manager, to pull a module down locally, then `import` it like any other file:

```bash
qpm add username/repo
```

```quill
import "qpm_modules/repo/lib.qsc";
```

(Adjust the path to match wherever `qpm` places the module on your machine.)

Modules are available for organizing larger programs; they are not the main focus of the language.

---

## Type System

Quill runs static type checking before generating native code.

| Type | Example |
| --- | --- |
| `number` | `42` |
| `double` | `3.14` |
| `string` | `"hello"` |
| `bool` | `true` / `false` |
| `char` | `'A'` |
| `void` | Functions with no return value |
| `number[N]` / `double[N]` / `string[N]` / `bool[N]` | `let stack: number[200];` |

---

## Building from Source

Requires **C++17** or higher.

### Windows (PowerShell)
Currently has no helper file due to complexity.

### Linux

```bash
git clone https://github.com/omo5454/Quill.git
cd Quill
./build.sh
```

---

## Project Structure

```
Quill/
├── src/
│   └── core/
│       ├── ast/          # AST node definitions
│       ├── lexer/        # Tokenizer
│       ├── parser/       # AST parser
│       ├── transpiler/   # Quill → C pipeline (current backend) + CLI
│       └── typechecker/  # Static type checker
├── stdlib/               # Math, stdio, and other small libraries
├── bin/                  # Output directory for compiled binaries
├── build.sh              # Linux build script
└── Readme.md
```

---

## Known Limitations

- `let`/`mut`/`const` don't enforce immutability yet.
- `elif` is reserved but not parsed — use `else if`.
- No array/struct function parameters or return values (arrays are local-only).
- No closures, function values, or higher-order functions — a function can only be called by name where it's written.
- No hashmaps/objects — see [Roadmap](#roadmap).
- `import` only works at the top level of a file; writing it inside a function or block is silently ignored.

---

## Roadmap

### Language & libraries
* [x] Fixed-size arrays (`number`/`double`/`string`/`bool`, indexed read/write)
* [x] `char` type and character literals
* [x] String, number, and char `input()`
* [x] File imports (`import "path.qsc";`)
* [x] `extern` function declarations for typed C interop
* [x] Raw C interop (`C_call` / `C_top`)
* [ ] Dynamic arrays / slice operations
* [ ] Hashmaps / Objects
* [ ] Extended string utilities (`split`, `trim`, `replace`)
* [ ] Richer math stdlib (vectors, matrices, more numeric helpers)
* [ ] Native File I/O (raw `<stdio.h>` functions are already callable via `extern`)
* [ ] Structured error handling (`try` / `catch`)

### Compiler backend
* [x] C++ transpiler pipeline (current: Quill → C → gcc)
* [ ] **LLVM backend** — compile Quill directly to LLVM IR / native machine code, dropping the C intermediate step for tighter optimization and a simpler runtime story
* [ ] Incremental improvements to the current C backend until the LLVM path is ready

### Tooling
* [x] Package manager (`qpm`) for optional GitHub-based modules
* [ ] Better diagnostics and error messages
* [ ] Formatter / basic IDE support

---

## License

Distributed under the **MIT License**.
