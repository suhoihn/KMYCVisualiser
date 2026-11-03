# KMY standard collection modules

This directory is a source-level standard-library experiment for the current
native x86 backend.  KMY has no generics yet, so each useful specialization is
an honest concrete module: `IntArrayList`, `StringArrayList`, `IntIntHashMap`,
and `StringIntHashMap`, rather than a misleading pseudo-generic API.

All resizable structures allocate replacement arrays and copy their contents.
The old arrays intentionally leak because the language has `malloc` but no
`free` or garbage collector yet.  Sentinel methods use `-1`, `false`, or an
empty string for absence; use `contains`/`isEmpty` where that ambiguity matters.

Run `tests/stdlib/smoke.kmy` through the guarded native test runner to exercise
the collection families together.
