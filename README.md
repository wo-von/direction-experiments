# Direction experiments: make it faster vs. design it vs. cluster scale

Plan source: "Direction experiments" tab under Roadmap in the *Job hunt 2026* doc
(https://claude.ai/artifact/8x6FRYRDyZwjNtAMo38YZ5). The doc is the plan; this repo holds the code and notes.

These are signal-gathering experiments, not portfolio projects. The output is the session notes
and a decision about the main track for Jan–Mar 2027.

| Weekend   | Experiment                                   | Dir                      |
|-----------|----------------------------------------------|--------------------------|
| Nov 7–8   | A: make it faster (SGEMM in C, perf)         | `weekend-a-sgemm/`       |
| Nov 14–15 | Buffer                                       |                          |
| Nov 21–22 | B: design it (accelerator for one layer)     | `weekend-b-accelerator/` |
| Nov 28–29 | Buffer                                       |                          |
| Dec 5–6   | C: cluster scale (all-reduce on loopback)    | `weekend-c-allreduce/`   |
| Dec 12–13 | Retrospective in chat: pick Track F / D / C  |                          |

- `notes/` — one file per session, written the same evening (template: `notes/TEMPLATE.md`).
- `conversations/` — notes from the three conversations with people in the roles.

Build standard (C): `-O2`/`-O3 -Wall -Wextra -Wpedantic`; sanitizer builds
(`-fsanitize=address,undefined`) for correctness checks only, never for timing.

## How we work (Claude + me)

The goal is to find out what I *like*, so the struggle with the core problem is the data. Friction that
isn't the experiment is waste.

- **Core work** (the kernel, the model, the all-reduce, reasoning about *why* it's slow): I write and
  debug it. Claude doesn't.
- **Concepts, readings, hardware, what a perf event counts:** ask anything.
- **Friction that isn't the experiment** (perf permissions, flags, plotting, intrinsic names): Claude helps immediately.
- **Scaffolding** (Makefile, timing/verification harness): Claude writes it when I ask.
- **Stuck on core work:** ~30 min alone, then the hint ladder (question → hint → pseudocode). One rung per
  ask; code only on "show me".
- **Review after it works — light:** only what changes the result or the conclusion (wrong output,
  untrustworthy measurement, a missed big idea). Language-lawyer nits only if they'd actually bite, and
  batched at the end, never mid-flow.
- **No decision tables or DECISIONS.md:** session notes are the only log. Design choices come up only when real.
- **Reference code** (OpenBLAS, NCCL): only at the step that says so.
- **xv6 labs:** MIT 6.1810 policy as written (no AI code or debugging), plus a post-mortem review once it passes.
