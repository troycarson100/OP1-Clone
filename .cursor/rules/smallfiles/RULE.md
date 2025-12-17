---
alwaysApply: true
---

# 🔒 Project Rule: File Size & Architecture Discipline

## CORE RULE (NON-NEGOTIABLE)

**No single source or header file may exceed ~500 lines of code.**

- If a file approaches **400 lines**, it MUST be split immediately.
- Cursor is NOT allowed to continue adding logic to oversized files.
- Refactoring into smaller files is REQUIRED before any new feature work.

Any file exceeding **600 lines** is considered a **failure** and must be split immediately.

---

## ONE RESPONSIBILITY PER FILE

Each file must have **one clear responsibility**:

- One class
- One system
- One UI component
- One DSP unit
- One manager or controller

If a file begins to:
- Handle multiple systems
- Mix unrelated logic
- Grow uncontrollably

➡ **Split it into multiple files. No exceptions.**

---

## REQUIRED MODULAR STRUCTURE

Large systems MUST be decomposed into multiple files.

### Example:
/DSP
CombFilter.h / .cpp
CombFilterDelayLine.h / .cpp
CombFilterFeedback.h / .cpp

/Sequencer
StepSequencer.h / .cpp
StepState.h / .cpp
StepRandomizer.h / .cpp

/UI
StepperComponent.h / .cpp
KnobComponent.h / .cpp
MacroAssignOverlay.h / .cpp


Cursor must NEVER collapse systems into a single “god file”.

---

## HEADER FILE RULES

Header files must be **minimal and focused**.

Allowed:
- Declarations
- Interfaces
- Enums
- Constants
- Small inline helpers

Forbidden:
- Large implementations
- Multiple unrelated classes
- Massive inline logic

Each `.cpp` must correspond to **one** `.h`.

---

## REFACTOR-FIRST RULE (CRITICAL)

When asked to:
- Add a feature
- Fix a bug
- Improve performance
- Refactor or optimize

AND the target file is near or over **500 lines**:

> **Cursor MUST refactor first** by creating new files  
> **before** adding or modifying functionality.

---

## EXPLICITLY FORBIDDEN

Cursor is NOT allowed to:
- Create or expand massive “Main”, “Processor”, or “Utils” files
- Add unrelated logic “for convenience”
- Say “keeping it simple for now” while bloating a file
- Leave TODOs instead of splitting files
- Merge DSP, UI, state, and parameter logic into one file

---

## NAMING & ORGANIZATION RULES

- File names must clearly describe their responsibility
- No generic dumping grounds like `Utils.h` or `Helpers.cpp`
- Prefer **many small files** over **few large ones**

Bad:

Utils.h (900 lines)

Good:
MathUtils.h
AudioBufferUtils.h
ParameterUtils.h


---

## CURSOR DECISION RULE

When unsure, Cursor must choose:

> **More files. Smaller files. Clear boundaries.**

---

## ENFORCEMENT SUMMARY

- Target file size: **≤ 500 lines**
- Soft limit: **400 lines**
- Hard failure: **600+ lines**
- Modular architecture is mandatory
- Refactor first, then extend

This rule applies to **all future changes**, refactors, and new files.




## Realtime Audio Safety (Mandatory)
- No memory allocations, container growth, string building, or file I/O in processBlock or anything it calls.
- No locks/mutexes/MessageManagerLock on the audio thread.
- UI ↔ DSP communication must use atomics or lock-free queues; DSP caches values once per block.
- Parameters that affect gain/time/filter/feedback must be smoothed to avoid pops/zipper noise.
- Randomization must be debounced/throttled and applied safely at block boundaries (no re-entrancy).
- Clamp all time/feedback/index values and guard against NaN/Inf.
