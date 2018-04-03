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

Functions can also be bound to native C++ functions. Consider our hash example from earlier; we can bind it to a ``Func`` instance like this:

.. code-block:: c++

    SEQ_FUNC seq_int_t my_hash_func(char *seq, seq_int_t len)
    {
        ...
    }

    ...

    Func h(types::Seq, Int, SEQ_NATIVE(my_hash_func));
    s | h() | print();

-----

:doc:`next <lambdas>`
