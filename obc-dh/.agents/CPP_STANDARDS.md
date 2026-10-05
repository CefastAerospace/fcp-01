# C++ Code Organization Standards

## 1. Architecture & Design Philosophy
- **Atomicity:** Keep functions simple, atomic, and single-purpose. Maintain a cohesive flow of information.
- **Simplicity over Frameworks:** Rely on general programming logic and C/C++ best practices. Do not introduce highly specific libraries or complex C++ frameworks unless absolutely indispensable to the project's core goal. Avoid over-engineering.

## 2. Code Sintax
### Naming Conventions:
You must strictly adhere to the following casing rules:
- **Functions and Classes:** `PascalCase` (e.g., `CalculateDependency`, `ReservationStation`).
- **Variables and Attributes:** `snake_case` (e.g., `instruction_queue`, `current_cycle`). Applies to local variables, class fields, and struct members.
- **Structs, Enums, and Enum Elements:** `UPPER_SNAKE_CASE` (e.g., `EXECUTION_STATE`, `OPCODE_ADD`).

### Function Syntax:
All function declarations and definitions MUST place each parameter on a separate line to ensure maximum readability and clean Git diffs.

```cpp
return_type function_name(
    type_1 parameter_1,
    type_2 parameter_2,
    ...
    type_n parameter_n
) {
    // Implementation
}
```

### Documentation Standards:
**Libraries and Local Helpers:**
- Non-basic library or system includes must have a concise inline comment naming the objects used from that header. Common foundational headers such as `<iostream>`, `<string>`, and `<vector>` do not require this comment.
- Local and static helper functions must have a concise comment explaining their purpose. When a helper uses a non-obvious mechanism, also summarize its main flow and invariant without narrating ordinary statements.

**Header Files (`.h` / `.hpp`):**
Use Doxygen-style block comments. Omit tags that are unnecessary (e.g., no `@return` for void functions, no `@details` for self-explanatory functions). The `@file` tag is strictly for the top-of-file general description.

```cpp
/**
 * @brief Brief description of the function/element.
 *
 * @details Complex explanation of the logic, if necessary.
 *
 * @param param_name Description of the parameter.
 *
 * @return Description of the return value.
 */
 ```
 
**Imports:** All the libraries should be imported on the header (the implementation only imports it's header - mantain the clean code). By every include, should be an inline comment about wich resources and funcions are effectively used (ex.: `#include <iosream> // para std::cin`).
 
**Implementation Files (`.cpp`):**
At the top of the implementation file (or before major module implementations), you MUST include this exact warning regarding the documentation:

```cpp
// ─── ATENÇÃO ──────────────────────────────────────────────────────
/*
 * O funcionamento detalhado das funções e as características dos
 * elementos desse módulo são abordados em "header.h".
 */
```

**In-line Comments:**
In the implementation, I like to put comments dividing the operations in the function (even if it's something more obvious - in wich case I give a shallow explanation). 
It's especially important in more complex expressions and snippets, or when using non-standard usage of library/framework functions. In this cases, I like to put a more general explanation and then more specifc scenes, or an deeper clarification about the topic. The sintax:

```cpp
// Shallow explanation.
// - Deeper explanation.
// - specific case 1.
// - specific case 2.
// ...
```

## 3. Separation of the Code & Formatting:
You must structure the source code visually using explicit ASCII titles and maintain strict ordering and syntax rules.

Visual Separators (Use exactly these strings):
For declarations in Headers (`.h` / `.hpp`):

- Dictionary for recurring technical jargon: 
`// ─── DICIONÁRIO ───────────────────────────────────────────────────`

- Classes: 
`// ─── CLASSES ──────────────────────────────────────────────────────`

- Structs: 
`// ─── STRUCTS ──────────────────────────────────────────────────────`

- Helper functions: 
`// ─── HELPERS ──────────────────────────────────────────────────────`

- Static elements/globals: 
`// ─── ELEMENTOS STATIC ─────────────────────────────────────────────`

For Implementation (`.cpp`), use the above where applicable, plus the following:

Before the start of any class implementation:

```cpp
// ==================================================================
// === CLASSE =======================================================
// ==================================================================
```

- Constructors: 
`// ─── CONSTRUTORES ─────────────────────────────────────────────────`

- Destructors:
`// ─── DESTRUTORES ──────────────────────────────────────────────────`

- Getters: 
`// ─── GETTERS ──────────────────────────────────────────────────────`

- Other Methods: 
`// ─── DEMAIS MÉTODOS ───────────────────────────────────────────────`

### Method Visibility & Ordering in Implementation:
- **Visibility Tags:** Precede every method definition in the .cpp file with a comment stating its visibility: `// Público:` or `// Privado:`.
- **Call-Stack Ordering:** Order methods logically based on how they are called. Implement a public method, immediately followed by the private methods it invokes (and the nested private methods those invoke), before moving to the next public method.
(Example flow: Public A -> Private C -> Private F -> Private D -> Private E -> Public B).
