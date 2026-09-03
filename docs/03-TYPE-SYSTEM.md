# Type System

Quill's type system is statically checked at compile time. All type errors are caught before code generation and execution.

## Primitive Types

| Type | Description | Example |
|------|-------------|---------|
| `number` | 32-bit signed integer | `42`, `-5`, `0` |
| `double` | 64-bit floating-point | `3.14`, `-2.5`, `1.0` |
| `string` | Text string | `"hello"`, `"Quill"` |
| `bool` | Boolean value | `true`, `false` |
| `char` | Single character | `'A'`, `'x'`, `'1'` |
| `void` | No value (functions only) | Used for functions with no return |

## Array Types

Fixed-size arrays are created with a literal size. The size must be known at compile time.

```quill
let numbers: number[10];    # Array of 10 integers
let values: double[100];    # Array of 100 doubles
let flags: bool[50];        # Array of 50 booleans
let names: string[5];       # Array of 5 strings
```

### Array Limitations (Current)

- Arrays cannot be function parameters (yet)
- Arrays cannot be returned from functions (yet)
- Arrays are local to the function they're declared in
- There is no dynamic resizing (they are fixed-size)

## Type Conversions

### Implicit Conversions

Some conversions happen automatically:

- `number` to `double` (widening)
- `number` to `string` (via string concatenation with `+`)
- `double` to `string` (via string concatenation with `+`)
- Any type to `string` (via `+`)

```quill
let n: number = 42;
let d: double = n;         # number → double (implicit)
let s: string = "Value: " + n;  # number → string
```

### Explicit Conversions

Use standard library functions for explicit conversion:

```quill
toString(42)     # number → string: "42"
```

### String Indexing

Indexing a string returns the character code as a `number`:

```quill
let s: string = "hello";
let code: number = s[0];   # Returns ASCII code of 'h'
```

## Type Annotations

Type annotations are optional but recommended:

```quill
# With annotations (recommended)
let x: number = 5;
func add(a: number, b: number): number {
    return a + b;
}

# Without annotations (Quill infers types)
let y = 10;
func multiply(a, b) {
    return a * b;
}
```

> **Recommendation:** Always use explicit type annotations in function signatures. They make code clearer and catch errors earlier.

## Type Checking

Quill performs strict type checking:

```quill
let x: number = 5;
let y: string = x;  # ❌ Error: cannot assign number to string

let z: double = x;  # ✓ OK: number can be widened to double
```

## Character vs Number

In Quill, `char` is distinct from `number`, but when you index into a string, you get a `number` (the character's code):

```quill
let ch: char = 'A';
let code: number = ch;       # This works

let s: string = "hello";
let code2: number = s[0];    # Returns number (character code)
```

## Numeric Operations

All arithmetic operations work on `number` and `double`:

```quill
let a: number = 10;
let b: number = 3;
let c: number = a + b;  # 13
let d: double = a / b;  # 3.333...
```

Division of two integers truncates:

```quill
let result: number = 7 / 2;  # 3 (truncated)
```

## String Operations

### Concatenation

Use `+` to concatenate strings and convert other types to strings:

```quill
let name: string = "Alice";
let age: number = 25;
let msg: string = "Name: " + name + ", Age: " + age;
```

### Length

```quill
len("hello")  # 5
len("")       # 0
```

### Conversion

```quill
toString(42)   # "42"
```

## Boolean Type

`bool` values are `true` or `false`. They result from comparisons:

```quill
let is_equal: bool = (5 == 5);     # true
let is_greater: bool = (10 > 5);   # true
let is_valid: bool = false;
```

Use in conditionals:

```quill
if is_valid {
    say "Valid";
} else {
    say "Invalid";
}
```

## Best Practices

1. **Use explicit type annotations** — Makes code clearer and catches errors early
2. **Prefer `number` for integers** — It's the default integer type
3. **Use `double` for floating-point** — More precision than `number`
4. **Use `string` for text** — Quill handles concatenation automatically
5. **Remember array limitations** — Plan your data structures around fixed-size, function-local arrays

## Next Steps

- [Language Features](02-LANGUAGE-GUIDE.md)
- [Standard Library](04-STDLIB.md)
