# XABL Design Decisions

Status: Working architecture record
Owner: Brierfield Labs
Language: XABL
Expansion: eXtensible Application Base Language
Source extension: .xabl
Licence: GPL-3.0-or-later

## Governing Principles

1. XABL is a modern continuation of dBASE/xBase, not a generic modern language with dBASE compatibility bolted on.
2. Preserve classic dBASE syntax, semantics, workflow, and mental model wherever practical.
3. Only diverge from historic dBASE/xBase behaviour when there is no technically sound alternative or a target platform genuinely requires it.
4. Maximum legacy compatibility is a hard rule.
5. Legacy source should run unchanged whenever technically possible. Compatibility engineering should happen beneath the application before source rewriting is considered.
6. Modern features should extend the dBASE model rather than replace it.
7. Classic and modern Studio workflows should both be available wherever practical.
8. Keep the base runtime and Studio small. Load compatibility, drivers, designers, and other capabilities as modules/extensions on demand.
9. Code, APIs, compatibility rules, file formats, architecture, and non-obvious internals must be fully documented.

## Language Identity and Syntax

- Language name: XABL.
- Source extension: .xabl.
- Case-insensitive language.
- Formatter defaults to lowercase keywords; uppercase and preserve-case modes are supported.
- Newlines terminate statements. Semicolons are not required as statement terminators.
- Preserve legacy semicolon continuation semantics where required.
- Keep explicit block endings such as endif, enddo, endcase, etc.
- Classic dBASE command syntax is the primary XABL style.
- Modern object/method syntax may exist where useful, but is secondary.
- One-based arrays/lists, matching xBase history.
- Legacy array declarations and version-specific limits are preserved by compatibility profile.
- Modern XABL can use dynamic/resizable collections while retaining one-based indexing.
- Implicit variables remain supported.
- var and const are available for explicit modern declarations.
- Optional type annotations and stricter checking are available, but dynamic typing remains normal.
- null and classic xBase empty-value behaviour are both supported with explicit rules.
- Support .T./.F. and true/false.
- Support .AND./.OR./.NOT. and and/or/not.
- Preserve classic date syntax/functions while adding modern date/time types.
- Preserve classic work areas, aliases, select/use semantics, and alias->field notation.
- Preserve macro substitution and dynamic expression behaviour for compatibility; modern projects may warn and offer safer alternatives.
- Keep classic dBASE scoping semantics such as public/private/local/parameters exactly where profiles require them.
- Modern XABL adds try/catch/finally.
- Modern XABL exposes async/await.
- Built-in collection types: list, map/dictionary, set.
- Specialised structures such as queue/stack live in the standard library.
- JSON, XML, CSV, regex, HTTP, process, env, config, secrets, logging, money, etc. are first-party modules imported explicitly.
- String interpolation uses square brackets inside strings, e.g. "Hello [name]".
- Literal interpolation brackets use doubled brackets.
- String/list slicing uses inclusive one-based ranges, e.g. values[2..4].
- Unicode-safe string indexing/slicing.
- Default and named function arguments are supported.
- Simple destructuring is supported but not central.
- Keep classic do case/endcase as the primary multi-way conditional.
- switch/endswitch may exist as an optional modern extension.
- for each is supported alongside classic for/next and do while.
- break and continue are supported but discouraged when clearer structured logic exists.
- Ternary ?: is supported but nesting should be discouraged.
- ?. and ?? are supported.
- No general function overloading.
- No user-defined operator overloading.
- Simple enums are supported.
- Lightweight records/structs are first-class and preferred over unnecessary classes.
- Classes/objects are optional, not the centre of the language.
- Single inheritance only; interfaces supported; composition preferred.
- No multiple inheritance.
- Interfaces are contracts only in v1.
- Simple properties/getters/setters are supported.
- Modern class visibility: public/private/protected.
- Lightweight optional generics only; no template-metaprogramming style system.
- No dedicated advanced pattern-matching construct in v1.
- decimal is the core exact numeric type.
- money is a first-party library/runtime type.
- bigint and bigdecimal are available explicitly.
- integer is the friendly default integer type; fixed-width signed/unsigned integer types exist for interop.
- Proper bytes/blob types; text and binary remain distinct.
- UTF-8 is the default encoding, with explicit legacy encodings available.

## Database and dBASE Behaviour

- The dBASE record-oriented model remains primary.
- Core commands such as use, seek, replace, locate, go, skip, set filter, browse, etc. remain central.
- SQL is secondary and optional for modern backends.
- Do not turn XABL into an ORM/SQL hybrid.
- Classic work areas, aliases, record navigation, indexes, filters, and locking remain first-class.
- The same dBASE-style workflow should work against DBF and modern backends where semantics can be mapped safely.
- Where a backend cannot reproduce historic behaviour exactly, Studio/runtime must make the difference explicit.
- Classic record/file locking controls remain available.
- Modern backends map those behaviours to the closest safe transaction/locking semantics with warnings where exact equivalence is impossible.

## Compatibility Profiles

Initial implementation order:
1. dBASE III PLUS
2. dBASE IV
3. Clipper
4. FoxPro
5. Visual FoxPro
6. Visual dBASE
7. dBASE PLUS / dBL
8. Harbour
9. xHarbour
10. Modern XABL

Each profile is independently testable and may load its own runtime compatibility module.

Compatibility includes:
- Source compatibility.
- Data compatibility.
- Behavioural compatibility.
- Version-specific quirks.
- Error behaviour/codes where practical.
- DBF and related memo/index formats.
- Legacy project/form/report/menu/resource formats where technically decodable.

Major data/index/memo formats include DBF, DBT, FPT, NDX, MDX, CDX, NTX and dialect-specific variants.

## Legacy Application Handling

- XABL should open every legacy xBase-family format it can technically decode.
- Opening support may be broader than safe round-trip editing support.
- Undocumented/partial formats should still be identified, preserved byte-for-byte, and surfaced read-only or through compatibility wrappers where needed.
- Legacy Project Import/Inspection wizard scans the whole application before conversion.
- Wizard identifies dialect/version, formats, unsupported constructs, and dependencies.
- Compatibility report uses Green/Amber/Red/Grey classifications:
  - Green: runs unchanged.
  - Amber: runs with compatibility layer or minor adjustment.
  - Red: manual intervention required.
  - Grey: not yet understood/supported.
- Compatibility report persists inside the project and updates as modernisation proceeds.
- Projects may freeze their compatibility profile/quirks.
- XABL Studio should generate compatibility-baseline tests for legacy applications.
- Original legacy source/files are preserved untouched alongside modernised counterparts.
- Studio tracks mapping between original and modernised files.
- Side-by-side execution/comparison of legacy and modernised versions is supported where practical.
- Compatibility shims may emulate missing legacy behaviour without changing source.
- Legacy code should run using whichever mechanism is most appropriate: exact semantics, runtime shims, adapters, emulation, VM execution, native compilation, or source translation only when necessary.
- Legacy source can compile into modern standalone applications where semantics can be preserved.
- Both bytecode/VM execution and native compilation are supported.
- Mixed-mode projects are first-class: legacy and modern XABL files may coexist.
- Per-file/per-module compatibility profile overrides are supported.
- Studio offers behaviour-preserving modernisation with preview/diff before changes.
- Legacy projects may:
  - open in place,
  - use an XABL wrapper,
  - or migrate a copy.
- Saving can remain in legacy format where safely round-trippable or convert to XABL-native format, with advice shown to the user.

## XABL Studio UX

Global rule: every major interface should offer Classic, Modern, and Hybrid modes wherever practical.

- Classic mode prioritises dBASE-style windows, workflows, terminology, and object organisation.
- Modern mode uses contemporary tabs, docking, search, inspectors, command palettes, etc.
- Hybrid combines dBASE concepts with modern window management and convenience.
- Mode changes presentation/workflow only, not language semantics.

Studio must include:
- Classic dBASE-style Command Window.
- Interactive Browse/table grid.
- Visible current work area and open aliases.
- Classic object-centric Navigator.
- Modern filesystem/project tree.
- Form Designer.
- Report Designer.
- Properties/Inspector.
- Debugger.
- Menu/toolbar designers where applicable.
- Query/database tools.
- Application Generator.
- Classic and modern interface modes for the above wherever practical.
- Form/Report designers support both docked and detachable/full-workspace modes.
- Form designer follows the classic "place controls visually and bind them to fields" model, with modern responsive/accessibility/layout capabilities layered on top.
- Studio shows the active environment/profile visibly, likely in the status bar.
- The environment indicator should be clickable and show active compatibility profile, modules, backend, format support, warnings, and frozen compatibility state.

## Modular Architecture

- Base Studio/runtime kept deliberately small.
- Capabilities load as plugins/extensions/modules as needed.
- Compatibility profiles implemented as modules such as:
  - xabl-compat-dbase3
  - xabl-compat-dbase4
  - xabl-compat-clipper
  - xabl-compat-foxpro
  - xabl-compat-vfp
  - xabl-compat-harbour
  - xabl-compat-xharbour
- Database drivers, report engines, web/server tooling, specialised designers, etc. should also be modular where practical.
- Lazy loading preferred.
- Users can pin modules where predictable startup is preferred.
- First-party and third-party components use one common extension architecture.
- Trust levels, signatures, provenance, and capability permissions control what extensions may do.
- Per-project module bundles/profiles declare required environment components.
- Manifest plus lockfile make project environments reproducible.
- Studio can automatically reconstruct the required environment on another machine.
- Offline module caches and exportable environment packs are supported.
- Portable Studio bundles are supported.
- Fully self-contained archival snapshots are supported.
- Archival snapshots may include source, untouched originals, data/schema metadata, modules, runtime/toolchain versions, dependencies, tests, docs, and build artifacts.
- Archival snapshots should use checksums/signatures for later integrity verification.

## Project and Package Model

- Human-readable project manifest in TOML.
- Manifest is the source of truth for dependencies, targets, compatibility profile, permissions, packaging, and environment.
- Semantic version constraints plus an automatically maintained lockfile.
- Core standard library tracks XABL language/runtime version.
- Optional first-party packages may version independently.
- Core/official imports may use short names.
- Third-party packages use owner namespaces.
- Packages are declarative by default.
- Executable install/build hooks are restricted, visible, permissioned, and sandboxed where practical.
- Simple projects default to main.xabl.
- Larger projects may define target-specific entry points for desktop, server, terminal, web, etc.
- Build/deployment profiles: development, test, staging, production.
- Profile overrides must be explicit and inspectable.
- Hot reload/live reload for safe changes; restart required when changes cannot be applied safely.

## Runtime, Tooling, and Application Targets

- XABL owns the language front end, compatibility semantics, intermediate representation/bytecode, virtual machine, runtime behaviour, and debugger.
- LLVM is the native-code generation backend for release/native builds.
- LLVM is used as a code-generation engine only; it does not define XABL language semantics.
- Development/debugging may favour the XABL VM/bytecode path for fast iteration, debugging, hot reload, sandboxing, and faithful compatibility behaviour.
- Native builds use LLVM optimisation/code generation for high-performance Windows/Linux binaries and future ARM64 support.
- Docker/OCI containers are an official XABL deployment target.
- Official container families should include minimal runtime, server, and SDK/build images.
- Preferred registry is GitHub Container Registry under ghcr.io/brierfieldlabs/.
- Project manifests/lockfiles should allow containers to reconstruct the exact required XABL runtime, compatibility modules, drivers, and extensions.
- OCI compatibility should allow use with Docker, Podman, Kubernetes, CI systems, and other standards-compatible runtimes.
- Multi-architecture images should support amd64 from the outset and arm64 when the runtime is ready.
- Implementation language: C++23.
- UI: Qt 6, Qt Widgets first.
- CMake + Ninja.
- Core/runtime must be UI-independent.
- Desktop GUI and CLI are separate applications over shared libraries.
- Major targets include Windows and Linux from the start.
- Web, server, runtime, CLI/TUI, and SDK are part of the product family.
- Mobile remains architecturally possible for later.
- Standalone applications must not require Studio to be installed.
- Server/service deployment is first-class.
- Built-in debugger is first-class.
- First-party LSP powers Studio and editor integrations.
- VS Code and Zed extensions are planned.
- Testing framework is first-class.
- assert is part of the language.
- test ... endtest is part of the testing toolchain.
- Structured logging is first-party.
- Documentation comments use /// and feed IDE help/API documentation.
- Small optional annotation system supports metadata such as validation, DB mapping, API routes, deprecation, tests, and plugins.
- Controlled reflection/introspection is supported.
- First-class events/event handlers are supported.
- Capability-based permissions apply to scripts, extensions, packages, jobs, filesystem/network/process access, databases, secrets, and deployments.

## Documentation

- Brierfield Labs attribution throughout.
- User Guide and Language Reference Manual are required.
- Online docs via GitHub Pages.
- Printable A4 manuals generated from the same documentation source.
- Developer documentation includes architecture, ADRs, file-format notes, compatibility docs, subsystem docs, and release notes.
- A language feature is not complete until documentation and tests exist.
- Codebase must be meaningfully commented and public APIs/non-obvious internals documented.

## Naming and Product Family

- XABL: language.
- XABL Studio: desktop IDE.
- XABL Server: server/headless runtime.
- XABL Web: web target/tooling.
- XABL Runtime: standalone runtime.
- XABL SDK: development SDK.
- Preferred project domain candidate: xabl.dev, subject to registration.

[executed on device: git-workbench-brierfield (07c1d400-9390-48f2-99c8-e7f1f531a7ee)]