Calling Python from Seq
=======================

Calling Python from Seq is possible in two ways:

- ``from python import`` allows importing and calling Python functions from existing Python modules.
- ``@python`` allows writing Python code directly in Seq.

In order to use these features, the ``SEQ_PYTHON`` environment variable must be set to the appropriate
Python shared library:

.. code-block:: bash

    export SEQ_PYTHON=/path/to/libpython.X.Y.so

For example, with a ``brew``-installed Python 3.8 on macOS, this might be
``/usr/local/Cellar/python@3.8/3.8.8_1/Frameworks/Python.framework/Versions/3.8/lib/libpython3.8.dylib``.
Note that only Python versions 3.6 and later are supported.

``from python import``
----------------------

Let's say we have a Python function defined in *mymodule.py*:

.. code-block:: python

    def multiply(a, b):
        return a * b

We can call this function in Seq using ``from python import``:

.. code-block:: seq

    from python import mymodule.multiply() -> int
    print multiply(3, 4)  # 12

(Be sure the ``PYTHONPATH`` environment variable includes the path of *mymodule.py*!)

Be sure to pass ``()`` to ``from python import`` regardless of the number of arguments the Python function takes;
Seq will pass all arguments at invocation time (e.g. ``multiply(1, 2)``). This is useful if the number
of arguments to a given Python functions is variable.

``@python``
-----------

Seq programs can contains functions that will be executed by Python via ``pydef``:

.. code-block:: seq

    @python
    def multiply(a: int, b: int) -> int:
        return a * b

    print multiply(3, 4)  # 12

This makes calling Python modules like NumPy very easy:

.. code-block:: seq

    @python
    def myrange(n: int) -> List[int]:
        from numpy import arange
        return list(arange(n))

    print myrange(5)  # [0, 1, 2, 3, 4]
