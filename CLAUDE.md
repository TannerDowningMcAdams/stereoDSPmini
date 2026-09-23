# stereoDSPmini

Two-MCU guitar pedal firmware in one superbuild:

- `H7/`: STM32H743, C++. Audio path, DSP engines, relays, dry VCA.
- `G0/`: STM32G030C8 (64 KB flash, 8 KB RAM, no FPU), C. UI board: pots, footswitches, mode switch, LEDs, MIDI in.
- `Protocol/`: C headers shared by both builds. The SPI contract between the boards.

## Documentation

`docs/` holds the design documents. It is gitignored, so it is absent from worktrees and fresh clones. If it is missing, ask for it before starting UI work.

- `docs/ui-application-plan.md` is the authoritative spec for the UI application. It overrides `docs/ui-application-brief.md` where they differ. Work is divided into milestones M1 to M7 with component IDs (P*, G*, H*). Stay inside the scope of the assigned milestone and components.
- `docs/ARCHITECTURE.md`: board split and protocol principles.
- `docs/g0-uart-bootloader.md`: H7 programming the G0 over UART (AN3155).
- `docs/REFACTOR.md`, `docs/multi-project-migration.md`: background.

## Build

Tools come from STM32CubeCLT only (`C:\ST\STM32CubeCLT_1.22.0`). Do not use other toolchains.

```bash
export PATH="/c/ST/STM32CubeCLT_1.22.0/CMake/bin:/c/ST/STM32CubeCLT_1.22.0/Ninja/bin:$PATH"
cmake --preset Debug        # first time only
cmake --build build/Debug   # builds G0, then H7 with the G0 image embedded
```

`build/Release` uses `-Os`. Report the G0 flash usage from the build output when a change adds code to the G0.

A change is verified when both images build without new warnings and any host tests pass. Behaviour on hardware is checked by the user on the bench.

## Hard rules

- Never flash, program, erase, or reset hardware, and never start a debug session against a target.
- Never commit. End each deliverable with a sample commit message of 1 to 2 sentences.
- Do not regenerate CubeMX projects or edit `.ioc` files. In CubeMX-generated files (`Core/`, `cmake/stm32cubemx/`), edit only inside `USER CODE BEGIN/END` blocks.
- Do not edit `Drivers/` or `Middlewares/`.
- List source files explicitly in `CMakeLists.txt`. No globs.

## Code conventions

- G0 stays in C11. H7 is C++ matching the existing style (`Config` struct plus `init(const Config&)`, accessors instead of public data).
- No heap. State is static and bounded by compile-time maximums. Board wiring goes in const tables.
- Keep HAL types out of public signatures. Confine HAL to the hardware-facing modules.
- ISRs do minimal work: latch, swap a buffer, or set a flag. Logic runs in thread mode.
- Data shared between an ISR and thread mode has one writer, and the handoff is explicit (`volatile` flags, `__DMB()` where ordering matters).
- Link events are levels, toggles, or counters, never single-frame pulses (plan §5).
- Match the surrounding code's naming, layout, and comment density.
- Aim for efficient, scalable, modular code where it earns its place. Do not add abstraction speculatively; the plan's "Level of abstraction" guidance applies.

## Writing style

These rules apply to code comments, docs, and messages.

- Clear, professional, technical. No promotional language.
- No em-dashes. No negative parallelism ("not X, but Y" constructions).
- Comments explain why, or a constraint the code cannot show. A comment longer than 2 lines inside a function body needs a specific reason to exist.
- Do not describe a choice as intentional or deliberate in a comment. State the reason.
