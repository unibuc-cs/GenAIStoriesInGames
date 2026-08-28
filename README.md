# Narrative Capability Contract — Unreal Prototype (v8)

A UI-heavy Unreal Engine prototype of **proposal-level authorization for
AI-assisted game authoring**, plus a small playable demo. It is a graphical
implementation of the interaction model the paper evaluated through a
structured terminal (Figure 3), populated with the hydro-station mystery
from Figure 4.

Everything is C++ (Slate UI, procedural demo scene) — **no binary assets,
no Blueprints, no maps**. The whole project is text, so it can be diffed,
reviewed, and regenerated.

## Requirements

- Unreal Engine **5.6+** (5.8 works: right-click `NarrativeContract.uproject`
  → *Switch Engine Version* → pick your engine, then open).
- A C++ toolchain for UE (Visual Studio 2022 with "Game development with C++"
  on Windows; Xcode on macOS).

## Build & run

1. Right-click `NarrativeContract.uproject` → **Generate Visual Studio project
   files** (or just double-click the `.uproject`; UE offers to build the
   missing module — answer *Yes*).
2. Open in the editor and press **Play** (the startup map is the empty engine
   `Entry` map; everything is created at runtime).
3. For a standalone feel, use *Play → Standalone Game* or package the project.

## What you get

### Authoring screen (opens on Play)

| Panel | Paper concept |
|---|---|
| Header | Active contract `C(L, D, M, B)` (Eq. 1), graph version, **policy selector** (Automatic / Assisted / Strict), *Play accepted graph* |
| Target curves (left) | Five-axis authoring view of Figure 2: genre prior (dashed), permitted ±ε band (shaded, ε = .20), bounded proposal (dotted), **draggable approved curve** (amber handle = deviation outside the band, logged, never concealed) |
| Inspectable history (left, lower) | Decision log (also exported to `Saved/DecisionLog.json`), curve edits, implementation needs, the Engine Capability Manifest |
| Story graph (center) | Figure 4's graph: node statuses, **red dashed blocked edge N6→N7 with its diagnostic**, dashed blue **scoped-revalidation boundary** after an accepted edit; click a node for its full contract record (preconditions, effects, bindings, mapping, provenance, branch state, path adherence per Eq. 12) |
| Authorization episodes (right) | Figure 3's five stages per episode: AI proposal, commitment profile, evidence package, authorization actions (approve / edit / substitute / defer / reject), accountable outcome |

Four episodes exercise the proposal classes from the paper:

- **E1 – Repair**: the blocked N6→N7 transition; approve inserting **N6b
  "Decode log"** (registered capabilities), weaken the precondition, or reject.
  Watch the scoped revalidation boundary cover only N6b–N9.
- **E2 – Normalization**: a registered label normalization. Auto-applies (and
  is logged) under Automatic/Assisted; requires review under Strict.
- **E3 – Capability**: "a crowd fills the turbine hall" — `CrowdSystem` has no
  manifest entry. Approve a *visible placeholder*, record an implementation
  need, or reject.
- **E4 – Deletion**: remove ending N9 — irreversible, global scope. Note what
  happens if you switch policy to **Automatic** while it is pending.

### Figure mode (v2)

For paper-ready captures, the header offers three utilities (with keybinds
that also work from anywhere):

- **Light theme / Dark theme** — switches the whole UI to a white "paper
  figure" palette (or back); semantic colors are re-tuned for contrast on
  each background.
- **Capture mode [F10]** — instantly hides every legend, hint line, and the
  utility buttons themselves, leaving only the interface content. Press F10
  again to bring the chrome back.
- **Screenshot [F9]** — runs `HighResShot 2` (a 2x-resolution capture) into
  `Saved/Screenshots/`.

Suggested figure workflow: switch to the light theme, stage the state you
want (e.g. select episode E1, or approve it to show the revalidation
boundary), press F10, then F9. Works in the demo too.

### Proposal sources: Curated and Live (v3)

The authorization panel has a source switch:

- **Curated** (default) — the reproducible episode queue described above.
  Deterministic, so it stays the mode for demos, screenshots, and reviewer
  walkthroughs. The same episodes double as fixtures for the automation
  tests (below).
- **Live LLM** — click a node in the graph (it becomes the expansion point),
  then *Generate proposal under &lt;node&gt;*. The tool sends a
  contract-grounded prompt (licensed predicates, registered capabilities,
  branch state, approved curve targets at the successor position) to an
  OpenAI-compatible chat endpoint, parses the strict-JSON reply, and runs it
  through the same extractor/validators as everything else: unsatisfiable
  preconditions fail the gate with a diagnostic, unregistered capabilities
  become placeholder decisions, unlicensed predicates are flagged as
  new-label commitments — and the result lands in the queue as a normal
  authorization episode, routed under the active policy.

Setup: copy `LLMConfig.example.ini` to `LLMConfig.ini` at the project root
and fill in `Endpoint`, `ApiKey`, `Model`. Local servers (Ollama, LM Studio)
work too — leave `ApiKey` empty. `LLMConfig.ini` is gitignored; never commit
a key. Malformed or slow replies only affect the status line — the graph
cannot be touched except through an authorized episode.

### Persistence (v4)

**Save** / **Load** in the header round-trip the complete state as JSON:
graph (nodes, choices), curves, manifest, episodes with their bounded
options and mutations, decision log, curve edits, implementation needs,
artifact versions, and policy. Files land in `Saved/ContractState.json`
plus a timestamped backup per save. On load, blocked flags and node
statuses are *recomputed* by the validator rather than trusted from disk.
The file is plain JSON — readable, diffable, and editable by external
tooling (e.g., to author new story briefs).

### Frontier expansion (v4)

In Live mode, **Expand frontier** runs one round of Algorithm 1's loop:
it picks up to three expansion points (live non-ending nodes with spare
choice slots, oldest paths first), asks the LLM for **three distinct
candidates** per point, scores them with a simplified Eq. 11
(target-curve fit + implementability + predicate licensing), and queues
the best per point as an authorization episode — the losing candidates
are recorded in the evidence package as scored, reviewed alternatives.
Growth is capped by a 16-node budget, and as always nothing enters the
graph without an authorized decision. The single **Generate proposal
under &lt;node&gt;** button uses the same candidates-and-ranking pipeline
for one expansion point.

### Story briefs (v7)

The **Briefs** header button opens a picker with three built-in worlds —
echoing the study's scenario families — plus any brief file found in
`<Project>/Briefs/`:

- **Hydro-Station Mystery** — the Figure 4 sample (default).
- **Dam Breach** (disaster) — the jammed-gate fact is learned in the
  control room, so the village branch's shortcut to the manual release is
  branch-locally blocked; `HelicopterRescue` is unregistered (placeholder
  episode); disaster-genre curves.
- **Derelict Station** (science fiction) — the sealed lab requires
  `power(restored)`, which only the reactor branch establishes; an
  irreversible ending-deletion episode; sci-fi curves.

Each world has its own predicates, capability manifest, target curves, and
curated episode — the same contract machinery, gate, demo, and Live
generation run unchanged on all of them. Switching briefs is undoable.

**Brief files are just Save files.** `Save current as brief` writes the
working state into `Briefs/<title>.json`; any `ContractState`-format JSON
dropped in that folder appears in the picker. That makes brief authoring an
in-app loop: build or generate a world, curate it, save it as a brief.

The scenario runner gains an eighth scenario, **Brief integrity**: every
built-in brief must build a well-formed world (no dangling edges, all nodes
reachable, a runtime-playable ending, and a branch-local blocked edge to
authorize).

### Undo as versioned rollback (v6)

Every decision (human or policy-automatic) pushes a full-state snapshot;
the amber **Undo (n)** header button rolls the last one back — including a
Load, and including the Automatic policy silently deleting an ending. Two
deliberate properties, matching Design implication 4: the decision log is
**append-only** (the rollback appears as its own `Rollback` record; the
undone decision stays in the log), and the graph version moves **forward**
on rollback, never rewinds — an undo is a new versioned transformation,
not an erasure. The undo stack is session-scoped (up to 20 deep, not
saved to disk).

### Session telemetry (v6)

The prototype now records the raw interaction stream the RQ2 analysis was
built on: `episode_selected`, `evidence_panel_opened` (which collapsible
evidence view, for which episode/node), `node_selected`, `decision`,
`policy_changed`, `generate_requested`, `save`/`load`, `undo`,
`demo_entered`/`demo_exited`, `node_executed`, and `runtime_blocked` —
each with session-relative time. Exported continuously to
`Saved/SessionTelemetry.json`; segmentation into authorization episodes
is left to offline analysis, as in the study. The history panel shows a
live event count.

### In-app scenario runner (v5–v6)

The **Tests** button in the header opens a scenario panel: a list of
end-to-end functional scenarios, run on click, with per-step PASS/FAIL and
details. Eight ship with v7:

1. **Full authoring loop (mock generator)** — frontier selection → three
   ranked candidates → episode routed for review → authorization → scoped
   revalidation → JSON save/load → runtime gate parity → replay to an
   ending. This is the whole pipeline in one click.
2. **Figure 4 repair loop** — diagnosis, N6b insert, exact boundary, replay
   through the repaired branch.
3. **Policy routing sweep** — the Automatic/Assisted/Strict matrix.
4. **Placeholder pipeline** — CrowdSystem to a visible production commitment.
5. **Irreversible deletion guard** — playable before, review-gated, gone after.
6. **Undo as versioned rollback** — reproduces the observed Automatic-policy
   incident (silent ending deletion) and rolls it back with history intact.
7. **Brief integrity** — every built-in world is well-formed and playable.
8. **Persistence round-trip** — edits survive; corrupt input refused.

Scenarios run on an **isolated contract copy** (never your session state;
persistence steps round-trip through strings, never your saved file), and
generation uses a **deterministic mock generator** flowing through the same
`RankAndBuildEpisode` path as the real LLM — reproducible in a talk, safe to
run mid-session. The identical scenarios also run headless as
`NarrativeContract.Scenarios.AllScenariosPass` in the automation suite.

### Automation tests (v3–v5)

The curated episodes have known expected outcomes, so they double as a
regression suite for the gate, routing, mutation, and revalidation logic
(`Source/NarrativeContract/Tests/ContractTests.cpp`), plus network-free
tests of the live-proposal parser and validator on canned replies.

Run from the editor: *Tools → Session Frontend → Automation*, filter
`NarrativeContract`, run. Headless (CI-friendly):

```
UnrealEditor-Cmd.exe <path>\NarrativeContract.uproject ^
  -ExecCmds="Automation RunTests NarrativeContract; Quit" ^
  -unattended -nopause -nullrhi -log
```

### Demo presentation (v8)

The playable demo is now a directed run rather than free exploration —
matching the player study's *selected sequences*:

- **Dialogue choices**: executing a node with several choices opens a
  choice prompt; press **1** or **2** to select. Only the chosen successor
  unlocks, so a run is one path through the graph. Choice guards are
  re-checked against live facts at selection time (a guard-unsatisfied
  option is marked and refused). Selections are recorded in the runtime
  trace and telemetry (`choice_selected`).
- **Procedural cast and props**: character stations spawn a basic-shapes
  NPC named from the node's entities (Ilya waits in the turbine hall; a
  small crowd gathers for evacuation scenes); stations grow accent props
  keyword-matched from their bindings (locker, log pedestal, valve wheel,
  scanner orb, reactor column, sealed door). Still zero assets.
- **Per-genre atmosphere**: height fog, key/fill light colors, and floor
  tint are keyed to the brief's genre — warm-teal mystery night, storm-grey
  disaster, cold-blue/red-accent science fiction.
- **Ending card**: reaching an ending shows a summary — ending text, the
  executed path, facts established, and the run's adherence to the
  approved curves — before Tab returns to authoring.

### Playable demo (*Play accepted graph*, or Tab)

A clean-state execution of the **current accepted graph** — stations spawn per
node (blue = available, green = executed, red = runtime-blocked, gray =
locked). WASD + mouse to move, **E** to execute a station, **Tab** to return.

- Before approving E1: the confrontation is runtime-blocked on the locker
  branch — the engine enforces the same precondition the validator flagged.
- After approving E1: N6b appears, and the full path to both endings plays.
- After approving E3's placeholder: N7 carries an explicit
  `[PLACEHOLDER: CrowdSystem]` marker — production commitments stay visible.
- The HUD shows branch state (runtime facts), runtime evidence, and the axis
  trace vs the approved target curves; the trace is exported to
  `Saved/RuntimeTrace.json` (runtime conformance, RQ3).

## Code map

```
Source/NarrativeContract/
  Core/ContractTypes.h      # nodes, choices, curves, episodes (Eq. 1–14)
  Core/ContractModel.*      # EdgeValid gate, routing policy, mutations,
                            # scoped revalidation, decision log
  Core/SampleData.cpp       # Figure 4 story, Figure 2 curves, episode queue
  Core/LlmProposals.*       # live source, network-free half: prompts,
                            # candidate parsing, Eq. 11 ranking, validators
  Core/ContractSerialization.* # full JSON save/load of the contract state
  Core/Scenarios.*          # end-to-end functional scenarios (UI-free)
  Core/Briefs.*             # built-in story worlds + JSON brief discovery
  UI/                       # pure Slate: curves, graph, authorization, log,
                            # main screen, demo HUD, collapsible sections
  Demo/                     # GameInstance (model + the HTTP half of the
                            # live source), GameMode/PlayerController,
                            # procedural scene + interactable stations
  Tests/ContractTests.cpp   # automation suite over the curated fixtures
```

## Notes & known limitations (v1)

- Input uses classic action mappings (`Config/DefaultInput.ini`); if a future
  engine version removes legacy input, rebind Tab/E via Enhanced Input.
- Station meshes tint via the `Color` parameter of the engine's
  `BasicShapeMaterial`; if a version renames it, state is still legible from
  the label + light colors.
- Frontier rounds are sequential (one HTTP request per expansion point) and
  bounded to three points per click; repeated clicks keep growing the graph
  up to the node budget.
- Undo covers decisions and loads; direct curve-handle drags are not on the
  undo stack (they are directly re-editable, and every edit is logged).
- Multi-parent branch-local state views are not yet implemented.
