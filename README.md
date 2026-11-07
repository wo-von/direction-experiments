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
