Internals
=========

The workflow is as follows:

- User specifies pipelines via a ``Seq`` instance
- User specifies input source
- Pipelines type-checked and validated
- Pipelines JIT'ed by LLVM
- Runtime reads and parses input, and passes to JIT'ed pipeline

Because sequences can have a lot of metadata associated with them (e.g. identifiers, quality scores, etc.), this data is also passed through the pipeline and is thus available if needed. Internally, each piece of data has an associated key, and is passed through the pipeline as a key+value pair.
