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


## UI, Designer, and Report Model

- Forms use a platform-neutral XABL UI definition rather than being tied directly to Qt or any one renderer.
- The same XABL form model can be rendered by desktop, web, terminal/TUI, and future targets where practical.
- Legacy form import should translate into the stable XABL form model instead of binding imported forms directly to a platform toolkit.
- Reports use a platform-neutral XABL report definition.
- Report definitions can render to preview, printer, PDF, HTML, and other suitable outputs through renderer backends.
- XABL owns report semantics, compatibility behaviour, pagination rules, data binding, and report designer behaviour.
- Mature third-party rendering/layout/PDF/font/printing libraries may be used underneath only when licence-compatible with GPL-3.0-or-later and the XABL distribution model.
- Project files, forms, reports, menus, and other designer assets should be plain text wherever practical.
- Human readability is a hard requirement: favour shallow structures, meaningful names, minimal ceremony, and formats that are easy to navigate and edit manually.
- TOML is preferred for project/configuration metadata.
- Designer assets should use a simple XABL-native declarative text format rather than complex, deeply nested, or noisy serialization.
- JSON should not be the default hand-maintained project/designer format unless interoperability strongly justifies it.
- Designer files remain primarily declarative.
- Executable event-handler logic normally lives in .xabl source files and is referenced by name from designer definitions.
- Small declarative expressions may remain embedded in designer definitions where useful.

## Native Extension Boundary

- Ordinary XABL modules are preferred over native extensions whenever possible.
- Native extensions use a stable C ABI rather than a C++ ABI.
- Trusted first-party native modules may run in-process where appropriate.
- Higher-risk or third-party native extensions may be isolated out-of-process.
- Extension trust, signature, provenance, and capability policy applies equally to native and non-native modules.

## Server and Web Execution Model

- XABL Server supports both straightforward synchronous request handlers and async/background execution.
- Simple request/response handlers should remain simple and should not require an elaborate concurrency model.
- Async APIs, queues, scheduled jobs, workers, and background tasks are available when an application needs them.
- XABL Web supports both server-rendered applications and richer browser-side applications.
- Server-rendered behaviour is the simpler/default path.
- Browser-side JavaScript and/or WebAssembly may be used when richer client-side behaviour genuinely helps.
- The web model should preserve XABL application structure and language semantics rather than forcing users into a separate unrelated framework.

## Bytecode and Runtime Compatibility

- XABL bytecode is versioned and documented.
- The bytecode format may evolve and is not frozen forever.
- Studio/runtime must detect bytecode versions and either execute them within the supported window, upgrade them safely, or select an appropriate older runtime/environment.
- Historical runtime versions are retained in the archive/package system so old applications can request the exact runtime they were built and tested against.
- Studio automatically selects the runtime and compatibility environment declared by a project's manifest/lockfile.
- Multiple XABL runtimes can coexist side-by-side on the same machine.
- Studio itself should be decoupled from a project's runtime version so newer Studio versions can manage older projects without forcing upgrades.
- Dependency resolution is per project.
- Multiple versions of the same dependency may coexist when different projects require them.
- Project-local package/extension installation is the default.
- Global installation is reserved for genuinely shared tooling such as Studio integrations, SDKs, or user-wide utilities.
- Projects may vendor dependencies directly for archival, offline, air-gapped, or high-assurance use.

## Release and Backward-Compatibility Policy

- XABL has an LTS release track alongside normal releases.
- LTS releases receive longer security, compatibility, and runtime support.
- Backward compatibility is a core design constraint.
- XABL should avoid breaking existing source, bytecode, project files, data formats, designer files, extensions, and runtime behaviour wherever technically possible.
- Patch releases must not intentionally introduce breaking changes.
- Minor releases should remain source-compatible by default.
- Major releases may only introduce breaking changes as a last resort, after deprecation, compatibility shims, migration tooling, and preservation of older runtimes have been considered.
- Legacy compatibility profiles are even more conservative than modern XABL itself.
- Once legacy behaviour is implemented and validated, later releases must not silently reinterpret it.
- Historical runtimes, modules, and environment definitions should remain retrievable for long-term reproducibility.

## Security, Trust, and Permissions

- Package and extension manifests declare required capabilities up front.
- Capabilities may include filesystem access, network access, process execution, database access, native code, secrets access, and other privileged operations.
- Studio/runtime should enforce declared permissions wherever practical.
- Permissions are scoped per project rather than being only global machine/user switches.
- Projects declare what they need; users approve those capabilities locally.
- XABL supports a restricted legacy mode for old applications, allowing risky behaviours to be contained without rewriting original source.
- Unknown legacy projects open restricted by default until explicitly trusted.
- Trust state should be clearly visible in Studio.
- Signed projects and signed release artifacts are supported.
- Signing may apply to projects, packages, bytecode bundles, archival snapshots, and application builds where appropriate.

## Compatibility Certification and Upgrade Safety

- Each supported legacy dialect/profile has a formal compatibility certification suite.
- Certification suites cover syntax, runtime behaviour, data/index/memo handling, locking, errors, forms/reports where practical, and historical quirks.
- Compatibility claims are evidence-based and measurable.
- Compatibility results are published publicly by release/profile, including test coverage, pass rates, known limitations, and regressions.
- Studio includes a per-project compatibility dashboard showing legacy dependencies, active shims, file formats, runtime/profile versions, test status, and upgrade risks.
- Studio can snapshot a project's compatibility state before runtime/module/profile upgrades.
- After upgrade, XABL can rerun compatibility baselines and compare behaviour automatically.

## Dependency Governance and Supply Chain

- Every third-party dependency must have recorded licence, source/provenance, version, and licence-compatibility assessment before entering the core build.
- Official XABL releases should be reproducible builds wherever the target platform allows it.
- Release builds pin toolchains, dependencies, and build inputs.
- Where byte-for-byte reproducibility is not practical, the full build inputs and environment must still be traceable.
- Official releases include a Software Bill of Materials (SBOM) covering bundled libraries, modules, versions, licences, and provenance.
- Official builds include provenance/attestation metadata tying artifacts to source commit, toolchain, CI workflow, dependency set, and build environment.
- Official XABL release artifacts must be signed, including binaries, installers/packages, container images, SDK/runtime archives, and other official distribution outputs.

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

## Additional Core Decisions — 2026-10-09

### Everyday UX

- Versioning, provenance, signatures, security controls, fingerprints, and compatibility history must be robust underneath but not front-and-centre during normal development.
- The normal workflow should remain: open project, write code, run, debug, build.
- Advanced trust, compatibility, provenance, and archival details remain accessible through dedicated dashboards, environment indicators, project properties, CLI diagnostics, and release tooling.

### Artifact Verification and Builds

- Studio and CLI automatically verify downloaded runtimes, modules, packages, container metadata, and other artifacts before use.
- Verification covers signatures, checksums, provenance, compatibility, and publisher trust.
- Successful verification stays quiet; unsigned, altered, incompatible, or unknown artifacts are surfaced clearly.
- XABL supports an opt-in hermetic build mode for CI, release, archival, and high-assurance builds.
- Hermetic builds block undeclared network and filesystem dependencies.
- Successful builds carry a machine-generated environment fingerprint covering runtime, compiler, modules, dependency hashes, compatibility profile, target, and build settings.

### Stable CLI Contract

- The XABL CLI is a stable public automation interface.
- Core command families include run, build, test, restore, package, publish, format, lint, doctor, env, compat, and migrate.
- Human-friendly output is the default.
- Stable machine-readable output is available for Studio, CI, editors, and scripts.
- Machine-readable output has an explicit schema version.
- CLI exit codes are stable, distinct, fully documented, and tested.
- Stable automation surfaces, including commands, exit codes, structured schemas, manifest fields, lockfile behaviour, bytecode metadata, package metadata, and compatibility status codes, must have formal documentation and tests.

### Preservation Rather Than Deprecation

- Working public behaviour should not be deprecated merely because something newer exists.
- Old syntax, APIs, commands, workflows, formats, and behaviours remain supported wherever technically possible.
- If an older feature genuinely blocks architectural progress, move it behind a compatibility module/plugin/profile instead of removing it.
- "Legacy" or "superseded" may be used to guide new projects without making old behaviour unavailable.
- Studio and CLI automatically resolve required legacy modules for older projects.
- No released XABL version should disappear from the official archive.
- Preserve matching runtimes, SDKs, documentation, compatibility modules, package metadata, checksums/signatures, release notes, SBOMs, provenance data, and migration notes indefinitely.
- Old documentation remains browsable online and selectable in Studio alongside the matching runtime.
- Published package/module versions remain available indefinitely except where an unavoidable legal or critical security issue prevents distribution.
- Preserve the compiler/toolchain/build environment used for official releases so old releases remain rebuildable, not merely downloadable.

### Project Compatibility Contract

- Every project has a formal compatibility contract describing runtime, language level, compatibility profile, required modules, file formats, and behavioural guarantees.
- The compatibility contract is plain text, human-readable, version-controlled, and diffable.
- Material changes that widen or weaken compatibility guarantees require explicit acknowledgement.
- Projects maintain a compatibility history recording what changed, why, previous/new state, and whether validation passed.
- Compatibility history is append-only by default; corrections are added as later entries rather than rewriting the past.

### Debugging and Interactive Development

- The debugger combines classic dBASE-style interactive inspection with modern breakpoints, stepping, watches, call stack, locals, and source navigation.
- Database state is first-class debugger state: work areas, aliases, current record, filters, indexes, locks, and related runtime state are inspectable.
- While paused, developers may modify variables, work areas, record position, filters, and other live state through the debugger/Command Window.
- Studio clearly marks a paused session whose state has been modified.
- Edit-and-continue is supported for safe changes; unsafe changes fall back to a controlled fast restart while preserving useful state where practical.
- The Command Window is available even when no project is open.
- Scratch XABL files and temporary workspaces can run without a full project manifest and can later be promoted into a project.

### Data and Legacy System Understanding

- Studio includes a visual data dictionary/schema editor for DBF and modern databases.
- The schema editor covers fields, indexes, relationships, validation rules, and metadata.
- Safe round-trip editing of legacy structures is supported where the target legacy format can represent the change exactly.
- When exact round-trip is impossible, Studio explains the limitation and offers a compatible alternative or XABL-side metadata rather than silently altering semantics.
- Studio includes a visual relationship/data-model view spanning DBF tables, indexes, aliases, and modern database relations.
- Existing legacy applications can be scanned to infer tables, indexes, aliases, relationships, source references, and other architecture.
- Inferred relationships are visibly distinguished from directly observed facts.

### Legacy Analysis and Documentation

- Legacy Analysis is a specialised workspace, not normal front-and-centre UI.
- Studio can generate a complete documentation pack for legacy systems, potentially including source modules, procedures/functions, tables, fields, indexes, relations, aliases/work areas, forms, reports, menus, dependencies, entry points, call graphs, data flow, file formats, compatibility requirements, and unknown/ambiguous areas.
- Generated documentation clearly distinguishes directly observed facts, inferred relationships, ambiguous/unknown behaviour, and unsupported/undecoded artifacts.
- Documentation can be exported to Markdown/HTML and printable A4 PDF with diagrams and cross-references where useful.
- Studio generates source-level call graphs and data-flow maps.
- Static analysis can be supplemented by runtime tracing that records files, tables, procedures, forms, reports, indexes, and external processes actually touched during a session.
- Guided legacy walkthroughs can combine runtime traces with notes, screenshots, and observations to preserve institutional knowledge.
- Imported legacy projects can generate a concise "Start Here" onboarding guide with likely entry points, main tables, important workflows, dependencies, risky areas, and suggested first files to read.
- The onboarding guide is refreshable as Studio learns from static analysis, runtime traces, walkthroughs, annotations, and compatibility findings.
- Non-invasive developer annotations can attach knowledge to legacy artifacts without modifying original files.
- Annotation visibility can be personal/private, project-team, or documentation-visible.
- Named knowledge packs can bundle selected annotations, walkthrough notes, diagrams, onboarding material, compatibility findings, and documentation.
- Studio can compare legacy-system versions at the architectural level, not only by line diff.
- Change-impact analysis combines static analysis, runtime traces, annotations, and known relationships to show what a proposed modification may affect.

### Application Generation and No/Low-Code

- XABL includes a built-in application generator that can scaffold working applications from DBF sets or modern schemas.
- Generated applications may include navigation, browse/edit forms, validation, search/filtering, reports, and target-specific surfaces.
- DB-first and model-first workflows are equal citizens, with reconciliation tooling when schema and model drift.
- One project can generate desktop, web, terminal, and server/API targets while sharing logic, validation, data access, and workflows where practical.
- Generated and hand-written code stay clearly separated.
- Regeneration only modifies generator-owned areas.
- Generated applications include an ownership map distinguishing generator-owned, developer-owned, and safely customisable areas.
- Application-generator templates are reusable and can define CRUD layouts, navigation, validation, report styles, deployment defaults, and other conventions.
- Template packs use the normal XABL package/module system.
- XABL deliberately supports no-code, low-code, and full-code workflows.
- No-code output is never opaque: generated XABL source and designer definitions remain inspectable and editable.
- No-code projects can graduate into hand-written XABL without migration into a different application model.
- Reusable visual components and workflow blocks are supported and distributed through the ordinary package/module system.
- Visual API/integration, job/scheduler, workflow, and state-machine tooling are valid XABL capabilities, but should normally be loadable modules rather than core runtime features.

### General-Purpose Scope and Module Rule

- XABL is a general-purpose application language with strong database heritage; it is not limited to business software.
- Graphics/canvas, media/audio, charting, 2D game/application loops, device I/O, filesystem watching, raw networking, and similar specialist capabilities should be first-party where valuable but implemented as modules/plugins.
- Standing rule: anything not fundamental to parsing, core runtime semantics, compatibility, or basic application execution should prefer the loadable module/plugin system.
- Modules may load at project start or at runtime where safe.
- Runtime unloading is supported only for modules that explicitly declare themselves unload-safe.
- Development-time hot reload is supported where ABI/API compatibility and runtime state make it safe; otherwise use a controlled restart.
- Studio itself is extensible by modules, including panels, inspectors, designers, commands, status indicators, project tools, new file handlers, and specialist tooling.

### Core VM and Runtime Model

- The initial XABL VM is a documented, versioned stack-based VM.
- LLVM remains the native-code generation path for optimised release builds.
- Automatic memory management is used for ordinary XABL values and objects.
- Runtime memory management primarily uses reference counting with cycle handling underneath; ordinary XABL code does not manually allocate/free memory.
- Normal XABL execution is single-threaded by default.
- async/await handles I/O concurrency.
- Explicit workers/threads provide real parallelism when required.
- Shared mutable state is discouraged unless using clearly thread-safe structures.
- Exceptions are unchecked by default.
- The runtime uses a unified tagged value model for dynamic values, with specialised operations available when the compiler can prove types safely.
- Truthiness, empty/null behaviour, implicit coercion, scoping, and other legacy semantics are governed by the active compatibility profile.
- Modern XABL may have cleaner internal distinctions, such as null versus uninitialised, but these must never leak into a legacy profile in ways that break historical behaviour.
- Hard rule: maximum compatibility with each supported legacy version/dialect settles detailed legacy-semantic questions unless a technical impossibility forces a documented exception.