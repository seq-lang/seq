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

#### Grammar:

```make
# Seq Grammar

input:
   statement+ EOF

decorator: '@' dotted_name [ '(' [arglist] ')' ] NEWLINE
decorators: decorator+
decorated: decorators (classdef | funcdef)
funcdef: 'def' NAME parameters ':' suite
parameters: '(' [varargslist] ')'
varargslist: ((fpdef ['=' test] ',')*
              ('*' NAME [',' '**' NAME] | '**' NAME) |
              fpdef ['=' test] (',' fpdef ['=' test])* [','])
fpdef: NAME | '(' fplist ')'
fplist: fpdef (',' fpdef)* [',']

stmt: simple_stmt | compound_stmt
simple_stmt: small_stmt (';' small_stmt)* [';'] NEWLINE
small_stmt: (expr_stmt | print_stmt  | del_stmt | pass_stmt | flow_stmt |
             import_stmt | global_stmt | exec_stmt | assert_stmt)
expr_stmt: testlist (augassign (yield_expr|testlist) |
                     ('=' (yield_expr|testlist))*)
augassign: ('+=' | '-=' | '*=' | '/=' | '%=' | '&=' | '|=' | '^=' |
            '<<=' | '>>=' | '**=' | '//=')
# For normal assignments, additional restrictions enforced by the interpreter
print_stmt: 'print' ( [ test (',' test)* [','] ] |
                      '>>' test [ (',' test)+ [','] ] )
del_stmt: 'del' exprlist
pass_stmt: 'pass'
flow_stmt: break_stmt | continue_stmt | return_stmt | raise_stmt | yield_stmt
break_stmt: 'break'
continue_stmt: 'continue'
return_stmt: 'return' [testlist]
yield_stmt: yield_expr
raise_stmt: 'raise' [test [',' test [',' test]]]
import_stmt: import_name | import_from
import_name: 'import' dotted_as_names
import_from: ('from' ('.'* dotted_name | '.'+)
              'import' ('*' | '(' import_as_names ')' | import_as_names))
import_as_name: NAME ['as' NAME]
dotted_as_name: dotted_name ['as' NAME]
import_as_names: import_as_name (',' import_as_name)* [',']
dotted_as_names: dotted_as_name (',' dotted_as_name)*
dotted_name: NAME ('.' NAME)*
global_stmt: 'global' NAME (',' NAME)*
exec_stmt: 'exec' expr ['in' test [',' test]]
assert_stmt: 'assert' test [',' test]

compound_stmt: if_stmt | while_stmt | for_stmt | try_stmt | with_stmt | funcdef | classdef | decorated
if_stmt: 'if' test ':' suite ('elif' test ':' suite)* ['else' ':' suite]
while_stmt: 'while' test ':' suite ['else' ':' suite]
for_stmt: 'for' exprlist 'in' testlist ':' suite ['else' ':' suite]
try_stmt: ('try' ':' suite
           ((except_clause ':' suite)+
            ['else' ':' suite]
            ['finally' ':' suite] |
           'finally' ':' suite))
with_stmt: 'with' with_item (',' with_item)*  ':' suite
with_item: test ['as' expr]
# NB compile.c makes sure that the default except clause is last
except_clause: 'except' [test [('as' | ',') test]]
suite: simple_stmt | NEWLINE INDENT stmt+ DEDENT

# Backward compatibility cruft to support:
# [ x for x in lambda: True, lambda: False if x() ]
# even while also allowing:
# lambda x: 5 if x else 2
# (But not a mix of the two)
testlist_safe: old_test [(',' old_test)+ [',']]
old_test: or_test | old_lambdef
old_lambdef: 'lambda' [varargslist] ':' old_test

test: or_test ['if' or_test 'else' test] | lambdef
or_test: and_test ('or' and_test)*
and_test: not_test ('and' not_test)*
not_test: 'not' not_test | comparison
comparison: expr (comp_op expr)*
comp_op: '<'|'>'|'=='|'>='|'<='|'<>'|'!='|'in'|'not' 'in'|'is'|'is' 'not'
expr: xor_expr ('|' xor_expr)*
xor_expr: and_expr ('^' and_expr)*
and_expr: shift_expr ('&' shift_expr)*
shift_expr: arith_expr (('<<'|'>>') arith_expr)*
arith_expr: term (('+'|'-') term)*
term: factor (('*'|'/'|'%'|'//') factor)*
factor: ('+'|'-'|'~') factor | power
power: atom trailer* ['**' factor]
atom: ('(' [yield_expr|testlist_comp] ')' |
       '[' [listmaker] ']' |
       '{' [dictorsetmaker] '}' |
       '`' testlist1 '`' |
       NAME | NUMBER | STRING+)
listmaker: test ( list_for | (',' test)* [','] )
testlist_comp: test ( comp_for | (',' test)* [','] )
lambdef: 'lambda' [varargslist] ':' test
trailer: '(' [arglist] ')' | '[' subscriptlist ']' | '.' NAME
subscriptlist: subscript (',' subscript)* [',']
subscript: '.' '.' '.' | test | [test] ':' [test] [sliceop]
sliceop: ':' [test]
exprlist: expr (',' expr)* [',']
testlist: test (',' test)* [',']
dictorsetmaker: ( (test ':' test (comp_for | (',' test ':' test)* [','])) |
                  (test (comp_for | (',' test)* [','])) )

classdef: 'class' NAME ['(' [testlist] ')'] ':' suite

arglist: (argument ',')* (argument [',']
                         |'*' test (',' argument)* [',' '**' test] 
                         |'**' test)
# The reason that keywords are test nodes instead of NAME is that using NAME
# results in an ambiguity. ast.c makes sure it's a NAME.
argument: test [comp_for] | test '=' test

list_iter: list_for | list_if
list_for: 'for' exprlist 'in' testlist_safe [list_iter]
list_if: 'if' old_test [list_iter]

comp_iter: comp_for | comp_if
comp_for: 'for' exprlist 'in' or_test [comp_iter]
comp_if: 'if' old_test [comp_iter]

testlist1: test (',' test)*

# not used in grammar, but may appear in "node" passed from Parser to Compiler
encoding_decl: NAME

yield_expr: 'yield' [testlist]

```

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



