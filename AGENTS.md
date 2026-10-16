# XABL: coding-agent entry point

Applies to ChatGPT, Codex and any other authorised assistant. **Before changing code**, read this repository's README, relevant current documentation, open GitHub issues/PRs, the current authoritative branch and existing CI/release rules. Never rely only on an old chat or handover.

## Brierfield Labs house rules

Follow the [canonical Coding House Rules](https://github.com/brierfieldlabs/infrastructure/blob/main/procedures/coding-house-rules.md) (for authorised users) and the [public proportionate-engineering summary](https://github.com/brierfieldpathway/.github/blob/main/CONTRIBUTING.md). Existing stricter project-specific data-safety, approval and release requirements continue to apply.

- **Ideas are not instructions.** Work on approved requirements only. Park extra ideas in issues, rather than quietly implementing them.
- **Simplicity is mandatory.** Deliver the smallest useful, maintainable change. Reuse components; justify any new service, database, dependency, configuration layer or abstraction.
- **Security is proportional to actual risk.** Preserve necessary protections for private data, secrets, integrity and recovery without speculative lockdowns.
- **Tests must earn their existence.** Add focused evidence for changed behaviour; honour established CI without inventing extra pipelines or redundant tests.
- **Stay decided and keep moving.** Recheck accepted decisions before proposing a redesign. Escalate material new scope or risk; continue independent approved tasks.
- **GitHub is the durable record.** Work on the correct development branch, use PRs and appropriate checks, update issues and current instructions, push coherent work, and record the next action. Do not invent green CI or deployment success.

## Project-specific boundary

Legacy xBase/dBASE compatibility is non-negotiable. Check existing decisions and current GitHub issues before changing syntax, runtime, debugger or editor architecture. Keep the implementation incremental.

This file provides **direction, not a new workflow engine**. Do not add new enforcement services or large governance documents to implement it.
