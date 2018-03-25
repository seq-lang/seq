Lambdas
=======

Here's an example of how lambdas can be used, which builds on the previous example:

.. code-block:: c++

    seq::Seq s;

    const unsigned N = 10;
    Var m = s.once | Int[N];
    Lambda x;
    s.once | range(N) | m[_];
    s.once | range(N) | m[_] | lambda(2*x + 1) | print();

    s.source("input.fastq");
    s.execute();

This prints all the odd integers between 1 and 20. In general, ``2*x + 1`` can be any arithmetic expression in terms of ``x``. Recall that we can also use ``foreach`` to iterate over arrays:

.. code-block:: c++

    s.once | m | foreach() | lambda(2*x + 1) | print();

-----

:doc:`next <serialization>`
