# AI usage

The course handout allows AI tools if we acknowledge them and record the most important
prompts. One entry per use: date, tool, the prompt, and what we kept or changed.

## 1. Design plan (2026-11-10)

- **Tool:** Claude
- **Prompt:** drafted from the Part 1 handout and our proposal text.
- **Result:** "CPU Cache Simulator — Design Plan", for the team to review and revise.
- **Kept / changed:** _to fill in_

## 2. First implementation (2026-11-10)

- **Tool:** Claude Code (Claude Opus 5.5)
- **Prompt:** "read this plan and make our project", with the design plan attached.
- **Result:** the first version of all three workstreams: the core library (`src/core`), the
  command-line runner (`src/cli`), the Allegro GUI (`src/gui`), the unit tests (`tests/`),
  the sample traces, the CMake build and the README. Where the plan left a choice open, the
  decision is listed under "Choices made where the plan was open" in `README.md`.
- **Kept / changed:** _to fill in after we review it. Before we submit, each of us should be
  able to walk through `Cache::access` and both policies (see the Risks section of the plan)._
