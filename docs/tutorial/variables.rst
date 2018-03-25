Variables
=========

Variables can be used to refer to intermediate values in a pipeline. For example:

.. code-block:: c++

    seq::Seq s;

    Var a = s | copy();
    Var b = a | len();
    Var c = b | substr(1,1);
    c | print();
    Var d = b | copy() | revcomp();
    d | print();

    s.source("input.fastq");
    s.execute();

Note that operations on variables are executed in program-order. This is the key difference between pipeliness and variables: adding a stage to a pipeline will cause that stage to be executed when that pipeline is executed, but adding a stage to a variable will cause that stage to be executed in whatever order it appears in the actual program.

``_`` is a special variable that always refers to the output of the previous stage; we'll see an example of it in the section on arrays.

-----

:doc:`next <arrays>`
