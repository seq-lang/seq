Functions
=========

Functions can be declared and called like this:

.. code-block:: c++

    seq::Seq s;

    Func f(types::Seq, Array.of(types::Seq));
    f | split(32,1) | collect();  // function body

    s | f() | foreach() | print();  // call function

Note that functions do not necessarily need to take any input, nor do they need to return a value:

.. code-block:: c++

    Func f1(Void, ...);   // no inputs
    Func f2(..., Void);   // no outputs
    Func f3(Void, Void);  // no inputs nor outputs

-----

:doc:`next <lambdas>`
