# P6a4 - Location access research (design)

Status: implemented (2026-11-19)
Scope: docs only.

Questions: native args/return, key suffix, value type, table/predicate, and what the port lacks for script-visible parity.

Method: disassemble 0x14041634, read .data1 0x1408fbcc, inspect NativeServices/ScriptVm globals.

Deliverable: docs/research/p6a4-location-access.md with a blocked verdict and next checks; 0x7000008 stays unsupported.
