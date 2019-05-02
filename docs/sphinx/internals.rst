Compiler Internals
==================

Overview
--------

This document describes Seq's compiler internals (excluding parsing). This includes everything in the top-level ``compiler/`` directory.

After a Seq program is parsed, the result is an AST that is built of several types of objects:

  - ``Stmt``: Top-level statements like ``if``, ``for``, variable assignments, etc.
  - ``Expr``: Expressions like ``2 + 2``, ``f(10)``, ``a[i]``, etc.
  - ``Var``: Variables defined either by assignment, or implicitly by e.g. a ``for`` (as in ``for x in ...``)
  - ``Func``: Function (or generator) objects defined by the user
  - ``Type``: Every expression and variable has an associated type, be it ``int``, ``array[float]``, ``MyClass[str,bool]``, etc.
  - ``Pattern``: Patterns define how to match a value in a ``match`` statement (and are used nowhere else)
  - ``Block``: A list of statements

There are a couple other less frequently used objects in addition to these:

  - ``BaseFuncLite``: A lightweight wrapper around an LLVM function (basically a lambda that describes how to codegen the function in LLVM IR); useful for easily writing built-in methods or functions in LLVM IR.
  - ``SeqModule``: This represents top-level code, which gets wrapped in a ``main`` function at the IR level. A ``SeqModule`` instance is the final result of parsing a Seq program; it contains everything within it.

Note that ``Func``, ``BaseFuncLite`` and ``SeqModule`` all inherit from the ``BaseFunc`` class, which defines a relatively barebones function interface.

Each of these classes is described in more detail below, along with any unintuitive corner cases or gotchas. Afterwards, generics are described, and finally type resolution and code generation.

Statements
----------

The ``Stmt`` base class is defined in ``compiler/lang/stmt.cpp``, but the actual subclasses representing real statements are defined in ``compiler/lang/lang.cpp``.

Statement objects typically hold expression objects internally. For example, ``print 42`` would be converted to a ``Print`` object that internally stores an expression representing the literal ``42``. Note also that expressions can be statements on their own (e.g. function calls that do not return anything), for which ``ExprStmt`` is used.

Statement objects can also contain other statement objects. For example, a ``While`` object (representing a ``while``-statement) contains an expression representing the condition, but also a list of statements that appear in the body of the loop. Any time a statement contains one or more internal "scopes" like this, a ``Block`` object is used to store the internal statements. For example, ``While`` stores just one ``Block`` internally, but ``If`` can store arbitrarily many, one for each branch. ``Block`` is nothing more than a wrapper around a vector of statements.

Statement objects also have a ``loop`` flag indicating whether they represent loops. If so, during code generation these statements must expose two LLVM blocks: one to jump to when a ``break`` is encountered and one to jump to when a ``continue`` is encountered. This allows ``break`` and ``continue`` statements to be codegen'd easily by finding their innermost enclosing loops and branching to the appropriate block.

Expressions
-----------

All expressions inherit from ``Expr``, and are defined in ``compiler/lang/expr.cpp``. Expressions are very similar to statements in their implementation, with the main difference being that every expression has an associated type, accessible via the ``getType()`` method. While ``getType()`` is *usually* simple (e.g. the type of ``42`` is clearly ``int`` -- no extra work required), occasionally ``getType()`` needs to do nontrivial work to determine the type. An example of this is calling a generic function/method; the type of the call could depend on the input type of the function, so ``getType()`` would have to clone the function, realize its generic type parameter with whatever the call type is, then deduce the return type.

A few other notes on expressions:

- Every expression has a ``tc`` field pointing to a ``TryCatch`` object (subclass of ``Stmt``), representing the innermost enclosing ``try`` block of the expression (or null if there isn't any). This is purely to handle function calls within a ``try`` at the LLVM level. Basically, if you call a function in a ``try`` block in LLVM IR, and the function can potentially throw an exception, LLVM IR provides an instruction (``invoke``) for doing this, which takes as arguments the block to jump to if no exception is thrown *and* a block to jump to if an exception is in fact thrown. The latter block is given to us by the mentioned ``TryCatch`` object.

- Some fields of some expression objects can be modified as a result of calling ``getType()`` (although not in a way that should affect the user). This has to do with automatic type parameter deduction: if you call a generic function or instantiate a generic class without providing explicit type parameters, ``getType()`` must first deduce the type parameters. It does this by, for each type parameter ``T``, finding ``T`` in each argument type of the function via a DFS, then extracting the corresponding type from the types the function is being called with. So, calling a function ``f[T](array[T])`` as ``f(array[int](10))`` would perform a DFS in ``array[T]`` for ``T``, then extract the corresponding type in ``array[int]`` (or failing if this can't be done), which is ``int`` in this case, meaning we deduce ``T = int``. A new ``FuncExpr`` object with explicitly realized types is then created to replace the original unrealized one (note that a pointer to the original is also kept for cloning purposes, described below).

- Currently, the only times an expression contains a statement internally are comprehensions and generators (i.e. ``ListCompExpr``, ``SetCompExpr``, ``DictCompExpr``, ``GenExpr``). These all contain a single ``For`` object representing the "unrolled" generator loop. For example, ``[a for a in b if a < 10]`` would be converted to a ``For`` representing ``for a in b: if a < 10: pass``. This is nothing more than a convenient interface for creating these kinds of expression objects.

- The types of most expressions (including all unary/binary operations and index/slice expressions) are determined through the type the operation is being applied to. For example, when determining the type of ``a + b``, the type of ``a`` is searched for an ``__add__`` method applicable to the type of ``b``, and the output type of this method is returned. This is discussed in greater detail in the section on types.

Variables
---------

All variables in a program (implicit or explicit, local or global) are represented by a ``Var`` object, defined in ``compiler/lang/var.cpp``. This includes standard assigned variables (``a = 10``), ``for``-loop variables (``for a in ...``) and function arguments (``def f(a)``). This **does not** include functions themselves, however; these are represented with ``Func`` objects (described below). Variables are conceptually just a wrapper around an LLVM pointer. If a variable is marked as global, this pointer will represent a global LLVM value, otherwise it will be an LLVM ``alloca`` in the enclosing function. Variables also have types, which are usually set in the *type resolution* pass before code generation (described below); these types dictate how the variables will be translated to LLVM IR.

There is one peculiar case that requires the ability to "map" a variable ``v`` to another ``v'``, meaning any operation applied to ``v`` will be delegated to ``v'``. This case is code generation of generators. Generators are converted to functions internally: e.g. ``(a*2 for a in v)`` is transformed to ``def _implicit_gen(v): for a in v: yield a*2``, which is then called with ``v`` as an argument. The way this is done internally is by creating a ``Func`` object representing the implicit function, assigning the generator's internal ``For`` loop as the body of this function, and codegen'ing the function. This would work, if it weren't for the fact that variables appearing in the ``For`` are referencing variables *outside* the implicit function! In the earlier example, for instance, ``v`` would have been assigned before the generator expression, but when we codegen the implicit function, we want ``v`` to refer to *the function's argument*. This problem is solved by "mapping" the original ``v`` variable to the implicit function's argument, codegen'ing the function, then unmapping ``v``. (Note that, because nested generators are possible, we actually maintain a *stack* of mappings rather than a single one, but the idea is the same.)

Functions
---------

Functions (i.e. those defined with ``def`` in a program) are represented by ``Func`` objects, defined in ``compiler/lang/func.cpp``. For the most part, ``Func`` is a wrapper around a ``Block`` representing the function's body, and a list of ``Var`` objects representing its arguments. Function return types can be deduced automatically. This is achieved by passing a pointer to the corresponding ``Return`` or ``Yield`` object to the function anytime a ``return``/``yield`` statement is parsed within it; then the return type (and whether the function is really a generator) is deduced by looking at the type of the expression being returned/yielded. Functions can also be marked as "external" (e.g. ``extern c foo()``), in which case no body is associated with the function.

Regular functions store a "preamble" LLVM block (which is their first block in the IR); this is where enclosed local variables (including function arguments) codegen their ``alloca``. Generators additionally maintain several other blocks for general generator bookkeeping (these are all described in the `LLVM coroutine docs <https://www.llvm.org/docs/Coroutines.html>`_).

Lastly, function names are mangled in the LLVM IR, and include a combination of the base function name, argument type names, output type name, enclosing function name and enclosing class name (if the function is a method of some class). This is so a generic function doesn't produce duplicate names in the IR if called on different types.

Types
-----

Of all the classes, types are probably the most complicated. All types inherit from the ``Type`` class, and are located in ``compiler/types/``. Fundamentally, each ``Type`` is a wrapper around an LLVM type, with some extra data dictating how to do various operations on instances of that type. Some types are "abstract", meaning there can't be any variables of that type (e.g. ``Void``). Types can contain other subtypes internally: for example an array type stores the base type of the array, and a generic class stores its generic type parameters.

All types have the following fields:

- ``vtable.fields``: A map of string (field name) to a pair of integer (0-based field offset in structure) and ``Type`` (type of field).
- ``vtable.methods``: A map of string (method name) to ``BaseFunc`` (method of type).
- ``vtable.magic``: A vector of ``MagicMethod`` objects -- these are built-in magic methods. Each ``MagicMethod`` object stores a name (e.g. ``__add__``), a vector of argument types (excluding 'self'), an output type, and a lambda that codegen's a call to the method, given 'self' and a vector of arguments. Additionally, ``MagicMethod`` has a ``asFunc()`` method, which returns a ``BaseFunc`` object representing the magic method. This allows for cases where a magic method is not called, but is referenced (e.g. ``x.__add__``).
- ``vtable.overloads``: A vector of ``MagicOverload`` objects -- these are the magic methods defined in a Seq program. Each ``MagicOverload`` object stores a name (e.g. ``__add__``) and a ``BaseFunc`` object representing the method. Note that magic methods defined by the user *must not* be generic, as they can be overloaded (e.g. you can have two ``__add__`` methods that take different argument types).

Many operations, as codegen'd by various expression objects, are defined by magic methods. Returning to an earlier example: to codegen ``a + b``, the type of ``a`` is searched for an ``__add__`` method applicable to the type of ``b``, be it a built-in one (``MagicMethod``) or a user-defined one (``MagicOverload``). This method defines both how to codegen the addition *and* what the output type is.

Importantly, note that a "method" is really a combination of the function defining the method *and* the object the method is called on (i.e. 'self'). For example, after ``f = a.foo`` where ``foo`` is a method, we can call ``f()``; this works because ``f`` is not *just* a function, but a struct of ``a`` and ``foo``, meaning ``a`` can be implicitly passed as the first argument of ``foo``. If we obtained ``foo`` statically as in ``f = A.foo`` (where ``A`` is the type of ``a``), then there is no 'self' involved and ``f`` would be just a function.

Thankfully, fields are much simpler than methods: to obtain the value of a field, we just search for its name in ``vtable.fields`` and produce the appropriate LLVM instruction with the resulting offset.

A few other notes regarding types:

- Every type has ``getBaseType(i)`` and ``numBaseType()`` methods for retrieving inner types (e.g. ``int`` in ``array[int]``). This is used in the DFS during type deduction mentioned above, and also for generating string names of types.
- Types can be marked as "atomic", which means that the type, when converted to an LLVM type, will contain no pointers to heap-allocated data. This saves our GC from having to scan objects of the type when searching for pointers.
- Every type has a ``getID()`` method, which returns a unique integer identifier for the type. This is used for exception handling to determine if an exception type matches any ``catch`` clause.

Patterns
--------

Patterns are relatively unimportant in the grand scheme of things, as they only apply to ``match`` statements. Nevertheless, all pattern objects inherit from ``Pattern``, and are defined in ``compiler/lang/patterns.cpp``. Patterns define how to "match" objects of a given type, and can be composed to form new patterns (e.g. as in ``OrPattern``). Some patterns are "catch-all", meaning they will match anything; this is important as we want the cases of a ``match`` to be exhaustive.

Generics
--------

Functions and classes can be generic. General "generics" functionality, including type parameter realization and deduction, are factored out into a ``Generic`` class, defined in ``compiler/types/generic.cpp``. This class is effectively a wrapper around a vector of generic type parameters, which are implemented as special type objects that delegate all operations to another underlying type.

The core operation when realizing a generic object is "cloning", which is essentially deep copying. For example, to realize a generic function, the entire function is cloned, and the cloned type parameters are realized. Here is a concrete example:

.. code-block:: seq

    def f[T](x: T):
        return x + 1

Now if we call ``f[int](42)``, the entire function will be cloned to create a new function ``f2``:

.. code-block:: seq

    def f2[T2](x2: T2):
        return x2 + 1

and ``T2`` will be realized as ``int``. To support this, *all* objects used by the compiler (including all those described here) are cloneable via a ``clone`` method. This method takes as an argument the ``Generic`` object that initiated the clone, which itself does some bookkeeping to ensure we don't clone things twice (e.g. cloning two calls to a single function should produce only one cloned function at the end).

Cloning classes can be complicated if the class definition contains realizations of itself (e.g. using ``A[int]`` in a generic ``A`` class). To solve this, such realizations are deferred, meaning ``A[int]`` (if used within ``A``) will not cause an actual realization of ``A``, but will instead create a generic type object that defers this realization until it is actually needed. Function and method realizations are always deferred similarly.

Passes
------

Code generation is done in two passes:

1. **Type resolution:** Traverse the AST and set types of variables, and deduce function return types. Note that type resolution on functions can potentially fail if generic types are involved (e.g. how can we resolve the type of ``a`` in ``for a in x`` if ``x`` is generic?); in this case, we fail silently in the hope of being able to resolve types fully when the function is realized.
2. **Code generation:** At this stage, all types should be fixed, so we re-traverse the AST and do the actual codegen.

Adding new statements and expressions
-------------------------------------

Looking at how some existing statements/expressions are implemented is probably the easiest way to understand what goes into them. Nevertheless, statements require the following:

- Inherit from ``Stmt`` class
- Override ``resolveTypes()``: This method should perform any required type resolution and recursively call ``resolveTypes()`` on any internal ``Stmt``/``Expr``/``Func``/``Pattern`` object.
- Override ``codegen0()``: This method takes an LLVM basic block reference as an argument, and performs code generation for the statement. By the time it returns, the argument basic block reference should point to a basic block where subsequent code generation can resume.
- Override ``clone()``: This method should perform a deep copy of the given statement, and recursively call ``clone()`` on any internal Seq objects contained in the statement. The method takes a ``Generic`` object as an argument, which keeps track of which objects have already been cloned; implementations should generally check if a clone of the given statement already exists before actually cloning (check source for examples). Returns in this function should be done with the ``SEQ_RETURN_CLONE`` macro, which internally sets some other clone fields for source information etc.

Expressions are very similar:

- Inherit from ``Expr`` class
- Override ``resolveTypes()``: Same as above
- Override ``codegen0()``: Same as above, except this method returns an LLVM value representing the result of the expression (can possibly be null if type is void).
- Override ``getType0()``: Returns the expression's type. You can assume this gets called *after* ``resolveTypes()``. Note that if the expression type is fixed, it can alternatively be passed to the ``Expr`` constructor directly and this method need not be overriden.
- Override ``clone()``: Same as above

Of course, the lexer/parser must also be updated to create any new statement/expression objects: ``compiler/util/ocaml.cpp`` provides C stubs that get called from the OCaml lexer/parser to interface with the C++ objects; new statements/expressions should generally provide wrappers there as well.

If an error (be it a type error or something else) occurs in any of these methods, a ``SeqException`` with an appropriate error message can be thrown.

``try``-``except``-``finally``
------------------------------

Exception handling in Seq is implemented using `Itanium ABI zero-cost exception handling <https://www.llvm.org/docs/ExceptionHandling.html#itanium-abi-zero-cost-exception-handling>`_. The implementation is fairly similar to how Clang compiles ``try``-``catch`` constructs in C++. However, Seq also supports explicit ``finally`` blocks, which greatly complicates the exception handling logic.

The first complicated case occurs when ``try``-``except``-``finally`` statements are nested. Consider the following example:

.. code-block:: seq

    def foo():
        try:
            try:
                try:
                    try:
                        bar()  # raises C
                    except A:
                        print 'A'
                    finally:
                        print 'f A'
                except B:
                    print 'B'
                finally:
                    print 'f B'
            except C:
                print 'C'
            finally:
                print 'f C'
        except D:
            print 'D'
        finally:
            print 'f D'

The output is:

.. code-block:: none

    f A
    f B
    C
    f C
    f D

The call to ``bar()`` raises an exception of type ``C``, which is handled by the ``except C`` block. However, all prior ``finally`` blocks must be executed, so the exception really must be caught by the innermost ``except``, whereupon some logic needs to be applied to determine that the next two ``finally`` blocks need to be executed, then the ``except C`` handler, then the last two ``finally`` blocks.

To handle this case, the compiler uses the concept of a handler's "depth": when an exception is caught, the depth of its handler is determined based on the exception type (e.g. when ``C`` is caught by ``except A``, its handler depth is 2; if the exception had type ``A`` then the depth would be 0). Then, a total of that many ``finally`` blocks are executed, after which the appropriate handler is executed, which in turn branches to latter ``finally`` blocks. Internally, this is coordinated by storing a depth state for each nested ``try``-``except``-``finally`` statement, which is set before reaching the ``finally`` block.

Another nontrivial case is as follows:

.. code-block:: seq

    def foo():
        try:
            return 1
        finally:
            return 2

Surprisingly to some, this function should actually return ``2``, not ``1``. This means that ``return`` statements in ``try``-``except``-``finally`` cannot just directly return, but must instead first execute the appropriate ``finally`` block(s). An analogous situation arises with ``break`` and ``continue``, which similarly cannot terminate the current loop iteration without executing the necessary ``finally`` blocks. (This is *not* the case with ``yield`` however, since it does not transfer control out of the block in the same way.) To handle this, nested ``try``-``except``-``finally`` statements share a "state" that can be one of the following:

- ``NOT_THROWN``: Nothing unusual has happened; execute as normal
- ``THROWN``: An exception has been thrown and not caught; execute ``finally`` then resume exception
- ``CAUGHT``: An exception has been caught here; execute handler then ``finally``
- ``RETURN``: A ``return`` statement was executed; store return value for later, execute all necessary ``finally`` blocks, and only then return
- ``BREAK``: A ``break`` statement was executed; execute all necessary ``finally`` blocks within the enclosing loop, and only then break
- ``CONTINUE``: A ``continue`` statement was executed; execute all necessary ``finally`` blocks within the enclosing, and only then continue

``finally`` blocks use this state to determine what block to branch to next. Note that "depth" takes precedence here: if the depth is positive then the next ``finally`` block must be executed before considering the state.
