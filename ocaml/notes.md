# The Seq OCaml GOTCHAS page

<!--------------------------------------------------->
## Notation

- `"a"`: literal `a`
- `a`: single `a` item **only**
- `a+`: single or more `a` items 
- `a?`: optional `a` item
- `<...>`: grouping
  - e.g. `(<a: b>+)` can express `(a: b, c: d)`
- `<a | b>`: item `a` or `b`

<!--------------------------------------------------->
## Grammar

### Lexing and parsing (`lexer.mll` and `parser.mly`)

#### Gotchas:
- TODO

<!--------------------------------------------------->
## Expressions and statements

### Scoping
- **Block** is the list of statements encountered so far within block statements such as `if`, `case`, `def` or `for`
- **Scope** is a block augmented with parent blocks as well

#### Example:
```python
# block = scope = {}
x = 1
# block = scope = {x}
if foo:
   # block: {}
   # scope: {x}
   def fn(x):
      pass
   # block: {fn}
   # scope: {fn, x}
   while foo:
      y = 3
      # block: {y}
      # scope: {x, fn, y}
```

### Rules:
- Functions/class blocks inherit all scope (parent) functions and classes
  - Variables are inherited only via explicit call to `global`
- Type and class member resolutions are currently handled by C++ part, not by OCaml part
- Shadowing is done via `:=` operator
  - `for` and some patterns also perform shadowing for their bound variables 

### Expression ASTs (`expr.ml`)

#### Gotchas:
- `list`, `set` and `dict` types must be loaded from stdlib for list/set/dictionary expressions/generators
- `parse_gen`: Generator parsing all referenced local and global variables are captured and passed to `GenExpr`
  - Loop variables shadow existing variables (they **do not** modify them)
- `parse_binary`
   - [ ] **Python-style `0 < a < 5` is NOT supported**
- `parse_index` currently parses:
  - Single slice expression `lhs[a?:b?:step?]` 
    - [ ] `step` is currently **NOT** supported
  - Single lookup expression `lhs[expr]`
  - Type indices `lhs[type+]` as follows:
    - Constructors `<"array" | "ptr" | "generator">[type]` 
    - Callback `"function"[type+]` where first type is the return type and other types are argument types
    - `type_expr[type+]` for `type_expr` realization
    - `<func_expr | elem_expr | static_expr>[type+]` for realization parameter setting
    - Any other `lhs` will throw an error
  - [ ] **Only one index expression is allowed** unless all indices are types (e.g. `a[b, c]` is not allowed)
  
### Statement ASTs (`stmt.ml`)

#### Gotchas:
- `parse_expr`:
  - `"__dump__"` expression will dump a current context table to the debug output.
- `parse_assign`:
  - Supports multiple assignment for `x+ = y+` (e.g. `a, b = c, d`) as follows:
    1. Create temporary variables in order: <br />
       `t = y` ⟺ `t1 = y1; t2 = y2; ...`
    2. Assign temporaries in order: <br />
       `x = t` ⟺ `x1 = t1; x2 = t2; ...`
  - The following single assignments are supported:
    - `x = y`
    - `x.elem = y`
    - `x[idx] = y`
  - Fails if LHS is type or function
  - Shadowing can be indicated with `:=` operator
    - Everything that applies to `=` applies to `:=` as well
    - [ ] **Over-shadowed variable is NOT GC'd**
  - [ ] **`x+ = y+` will fail if `len(x)` ≠ `len(y)`** (e.g. unpacking such as `a, b = c` is not supported)
- `parse_del`: parses `"del" expr+`
  - `del x[idx]` will call `x.__delitem__(idx)` 
   - `del x` will remove `x` from context variable table
     - [ ]  **this does not call GC on `x`**
- `parse_print`, `parse_yield`, `parse_return`, `parse_assert`: parses `<"print" | "return" | "yield" | "assert"> expr+`
  - Supports multiple expressions (e.g. `print a, b, c`)
  - `print` only: separates each element with space `' '` and inserts newline `'\n'` at the end
- `parse_for`: parses `"for" var+ "in" expr`:
  - Supports multiple variables by assigning in order: <br />
    `var1 = expr[0]; var2 = expr[1]; ...`
  - Loop variables shadow existing variables (they **do not** modify them)
- `parse_match`: parses `"match" expr` with the following patterns:
  - nameless wildcard pattern `"default"`
  - named wildcard pattern `"case" name`
  - guarded pattern `"case" pattern "if" expr`
  - bound pattern `"case" pattern "as" var`: matches `pattern` and assigns result to `var`
    - **At most one bound pattern can be present within a match case**
    - Bound variables shadow existing variables (they **do not** modify them)
  - star pattern `"case" ...`
  - or pattern `"case" <pattern | >+ pattern`
  - range pattern `"case" int "..." int`
  - `int`/`bool`/`seq`/`str`/`tuple`/`list` patterns
- `parse_type`: parses `"type" name (<member: type>+)`
  - All members must have explicit types 
  - `name` must not be already defined in <u>*scope*</u>
- `parse_global`: parses `"global" var+`
  - raises exception if `var` is already local or set as global
  - calls `Var::setGlobal()` on `var`
- `parse_extern` parses extern FFI definition `"extern" lang <(dylib)>? name (<param: type>+)`
  - All members must have explicit types 
  - `name` must not be already defined in <u>*block*</u>
  - [ ] **Currently `lang` can be only `c` or `C`**
- `parse_function`: parses `"def" name <[generic+]>? (<param: type?>+) < "->" type >?` 
  - `name` must not be already defined in <u>*block*</u>
  - Unnamed generic parameters are assigned generic names formed by prefixing two backticks to the parameter names (e.g. ``` ``name ``` for parameter `name`)
    - **Not accessible by user**
- `parse_class`: parses `"class" name <[generic+]>? (<param: type>+)`
  - All members must have explicit types 
  - `name` must not be already defined in <u>*scope*</u>
  - Class members must be only:
    - Functions `def` 
    - Empty statements `pass`
- `parse_try`: 
  - allows only one `"finally"`
  - allows `"catch" type`, `"catch" type "as" var` and `"catch"`
  - all expressions within `try` get set with `expr->setTryCatch(try)`
    - [ ] **If multiple `try` statements are nested, each expression will be set to closest (i.e. deepest) `try` statement**
- `parse_import`: parses `"import" name`
  - Everything is imported into the current scope as/is
  - `name` resolution:
    - parse `name.seq` in the directory of the running script
    - if it fails, parse `${SEQ_PATH}/name.seq`
  - [ ] **Currently only supports simple imports** 



