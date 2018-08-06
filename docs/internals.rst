Internals
=========

Overview
--------

The user works directly with a ``SeqModule`` object:

.. cpp:class:: seq::SeqModule

The workflow is as follows:

- User specifies pipelines via a ``SeqModule`` instance
- User specifies input source
- Pipelines type-checked and validated
- Pipelines JIT'ed by LLVM
- Runtime reads and parses input, and passes to JIT'ed pipeline

Because sequences can have a lot of metadata associated with them (e.g. identifiers, quality scores, etc.), this data is also passed through the pipeline and is thus available if needed. Internally, each piece of data has an associated key, and is passed through the pipeline as a key+value pair.

Stages and Pipelines
--------------------

Pipelines are constructed from multiple ``Stmt`` instances linked together. Values are passed from one stage to another by virtue of a map from keys (representing value types) to the values themselves:

.. cpp:enum:: seq::SeqData

.. cpp:type:: seq::ValMap = std::shared_ptr<std::map<seq::SeqData, llvm::Value*>>

For example, we would pass a regular sequence using the following map:

.. code-block:: c++

    {
        {seq::SeqData::SEQ, seqValue},
        {seq::SeqData::LEN, lenValue}
    }

where ``seqValue`` and ``lenValue`` are both ``llvm::Value`` pointers.

**Importantly,** these values must be pointers (i.e. representative of pointers in the LLVM IR), such as ``alloca``'d memory or global variables. For instance, ``seqValue`` above would be a pointer to, say, some ``alloca``'d memory that itself contains a character pointer; ``lenValue`` would be a pointer to an integer value.

``Stmt``
~~~~~~~~~

Stages make up the pipelines and are where the actual code generation takes place.

.. cpp:class:: seq::Stmt

    Stmt class

.. cpp:member:: seq::SeqModule* seq::Stmt::base

   The ``SeqModule`` instance associated with this stage

.. cpp:member:: bool seq::Stmt::added

    Whether this stage has been added in any pipeline to any ``SeqModule``

.. cpp:member:: seq::types::Type* seq::Stmt::in

    Stmt input type

.. cpp:member:: seq::types::Type* seq::Stmt::out

    Stmt output type

.. cpp:member:: seq::Stmt* seq::Stmt::prev

    Pointer to previous stage

.. cpp:member:: std::vector<seq::Stmt*> seq::Stmt::nexts

    Vector of subsequent stages actually linked to this stage

.. cpp:member:: std::string seq::Stmt::name

    Name of this stage (primarily for debugging)

.. cpp:member:: llvm::BasicBlock* seq::Stmt::block

    The block to which this stage will be compiled

.. cpp:member:: llvm::BasicBlock* seq::Stmt::after

    The block following ``block``

.. cpp:member:: seq::ValMap seq::Stmt::outs

    Map of all output values for this stage

.. cpp:function:: virtual void seq::Stmt::codegen(llvm::Module *module)

    Generates LLVM IR for this stage and for subsequent stages.

.. cpp:function:: virtual void seq::Stmt::finalize(llvm::ExecutionEngine *eng)

    Performs any finalization actions on the LLVM execution engine (e.g. adding flobal mappings to call external functions).

``Pipeline``
~~~~~~~~~~~~

Pipelines are conceptually just a head stage and a tail stage. The ``Pipeline`` class is purely for convenience, and does not store any independent state information; all the data is in the ``Stmt`` instances.

.. cpp:class:: seq::Pipeline

    Pipeline class

.. cpp:member:: seq::Stmt* seq::Pipeline::head

    Head of this pipeline

.. cpp:member:: seq::Stmt* seq::Pipeline::tail

    Tail of this pipeline (rightmost in the case of branching)

Type System
-----------

Each of the various types inherits from the ``Type`` class:

.. cpp:class:: seq::types::Type

    Type class

.. cpp:member:: std::string seq::types::Type::name

    Name of this type

.. cpp:member:: seq::types::Type* seq::types::Type::parent

    Parent of this type

.. cpp:member:: seq::SeqData seq::types::Type::key

    Key associated with this type

The type classes also have several member functions for generating code for specific operations (e.g. load/store from array, serialization/deserialization, printing, creating and calling functions, etc.).

Functions
---------

The ``seq::SeqModule`` class is a subclass of ``seq::BaseFunc``, which is a generic wrapper around an LLVM function that is also used for defining Seq functions:

.. cpp:class:: seq::BaseFunc

    General function base class

.. cpp:member:: llvm::Module* seq::BaseFunc::module

    LLVM module associated with this function

.. cpp:member:: llvm::BasicBlock* seq::BaseFunc::initBlock

    Block to be executed *once* (over all invocations) at the start of the function

.. cpp:member:: llvm::BasicBlock* seq::BaseFunc::preambleBlock

    First block in the function; this is where (for example) ``alloca`` should go

.. cpp:function:: virtual seq::types::Type* seq::BaseFunc::getInType() const

    Function input type

.. cpp:function:: virtual seq::types::Type* seq::BaseFunc::getOutType() const

    Function output type
