# ADR: Tokenise expressions before emitting bytecode

Status: Accepted, 9 October 2026

## Context

The initial dBASE-compatible executable vertical slice used string
searches and recursive text splitting for expression parsing. The
implementation became fragile when dotted logical operators, unary signs,
alias-qualified fields and parenthesised expressions were introduced.

## Decision

Use a dedicated lexer and a small precedence-climbing parser for
expressions. Emit the existing stack VM instructions directly. Keep the
command-level compiler unchanged for now, and retain a private internal
interface between the expression subsystem and compiler.

## Consequences

This adds a separate source unit and more explicit malformed-input
diagnostics, without requiring an AST or changing the bytecode interface.
It corrects unary arithmetic binding (for example `-2 + 3`) and
left-associative arithmetic (`20 - 5 - 3`).

Historical semantics remain a compatibility obligation: do not infer that
passing tests means all dBASE dialect differences have been verified.
The future language-level parser may supersede this component after
compatibility-profile work.
