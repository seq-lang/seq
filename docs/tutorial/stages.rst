Stages
======

Pipelines consist of several stages chained together. Stages all have an input type and an output type. The following built-in stages are supported in ``seq::stageutil``:

+-----------------------+-----------------------+-----------------------+
| Stmt                 | Types                 | Description           |
+=======================+=======================+=======================+
| ``copy()``            | ``Seq`` → ``Seq``     | Copies input sequence |
|                       |                       | into a new buffer     |
+-----------------------+-----------------------+-----------------------+
| ``count()``           | ``Seq`` → ``Int``     | Counts how many items |
|                       |                       | are passed through    |
|                       |                       | the stage             |
+-----------------------+-----------------------+-----------------------+
| ``filter(fn)``        | ``Seq`` → ``Seq``     | Filters sequences     |
|                       |                       | based on ``fn``       |
|                       |                       | (which returns a      |
|                       |                       | boolean)              |
+-----------------------+-----------------------+-----------------------+
| ``hash(h)``           | ``Seq`` → ``Int``     | Computes hash         |
|                       |                       | function ``h`` on     |
|                       |                       | input                 |
+-----------------------+-----------------------+-----------------------+
| ``len()``             | ``Seq`` → ``Int``     | Returns length of     |
|                       |                       | input                 |
+-----------------------+-----------------------+-----------------------+
| ``op(fn)``            | ``Seq`` → ``Seq``     | Applies ``fn`` to the |
|                       |                       | input sequence in     |
|                       |                       | place                 |
+-----------------------+-----------------------+-----------------------+
| ``print()``           | ``Any`` → ``Any``     | Prints and propagates |
|                       |                       | the input             |
+-----------------------+-----------------------+-----------------------+
| ``range(a,b,step)``   | ``Void`` → ``Int``    | Generates the         |
|                       |                       | specified range of    |
|                       |                       | integers              |
+-----------------------+-----------------------+-----------------------+
| ``revcomp()``         | ``Seq`` → ``Seq``     | Reverse complements   |
|                       |                       | the input sequence in |
|                       |                       | place                 |
+-----------------------+-----------------------+-----------------------+
| ``split(k,step)``     | ``Seq`` → ``Seq``     | Splits input into     |
|                       |                       | ``k``-mers with step  |
|                       |                       | ``step``              |
+-----------------------+-----------------------+-----------------------+
| ``substr(pos,len)``   | ``Seq`` → ``Seq``     | Extracts the          |
|                       |                       | length-\ ``len``      |
|                       |                       | substring of the      |
|                       |                       | input at ``pos``      |
+-----------------------+-----------------------+-----------------------+
| ``foreach()``         | ``Array[T]`` → ``T``  | Returns each element  |
|                       |                       | of input array in     |
|                       |                       | order                 |
+-----------------------+-----------------------+-----------------------+
| ``collect()``         | ``T`` → ``Array[T]``  | Collects all inputs   |
|                       |                       | into an array         |
+-----------------------+-----------------------+-----------------------+
| ``chunk(f)``          | ``Array[T]`` →        | Partition input array |
|                       | ``Array[T]``          | by key function ``f`` |
+-----------------------+-----------------------+-----------------------+
| ``get(i)``            | ``Rec[..,T_i,..]``    | Extract the ``i`` th  |
|                       | → ``T_i``             | member of the input   |
|                       |                       | record                |
+-----------------------+-----------------------+-----------------------+
| ``ser(file)``         | ``Any`` → ``Void``    | Serializes input to   |
|                       |                       | ``file``              |
+-----------------------+-----------------------+-----------------------+
| ``deser(T,file)``     | ``Void`` → ``T``      | Deserializes          |
|                       |                       | type-\ ``T`` object   |
|                       |                       | from ``file``         |
+-----------------------+-----------------------+-----------------------+

-----

:doc:`next <variables>`
