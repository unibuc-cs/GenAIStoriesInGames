# User Guide — Narrative Capability Contract Prototype

A feature-by-feature walkthrough: what each part does, how to use it step
by step, why it exists (which claim or mechanism of the paper it
implements), and where it is useful — paper figures, talks, artifact
review, or future studies.

Press **Play** in the editor. Everything below starts from the authoring
screen that appears.

---

## 1. The authoring screen at a glance

Layout: **header** (contract versions, policy, utilities) · **left**
(target curves over inspectable history) · **center** (story graph +
selected-node record) · **right** (authorization episodes).

**Rationale.** This is the graphical implementation of the paper's Figure 3
interaction model. The evaluated prototype was a structured terminal; this
screen arranges the same five functional stages spatially, with the
division of responsibility from Figure 1: designer-controlled surfaces
(curves, authorization) flank the machine-maintained versioned graph.

The header line `active contract C(L3, D5, M7, B2) · graph v18` is Eq. 1:
the versions of the Core Narrative Library, Domain Narrative Profile,
Engine Capability Manifest, and Story Bible, plus the graph version. Watch
these advance as you act — every accepted change is a new version.

**Where to use it:** the overview screenshot for the paper's
graphical-prototype figure; the opening shot of any talk demo.

---

## 2. Target curves (left panel)

**What.** Five axes — valence, tension, agency, information, stakes — each
with a dashed genre prior, a shaded permitted band (prior ± 0.20), a dotted
bounded model proposal, and a solid approved curve with draggable handles.

**How.**
1. Click an axis tab (Valence … Stakes) to open its detailed view.
2. Drag a square handle vertically to edit the approved curve.
3. Drag a handle outside the shaded band — it turns **amber**. The edit is
   allowed but visibly deviant, and it is logged (actor, old/new value) in
   the history panel; the Story Bible version increments.
4. Click a node in the graph: the node record shows *path adherence*, the
   Eq. 12 fit of that node's path against your approved curves.

**Rationale.** Sec. 3.2 / Figure 2: intent lives in approved curves, not in
prose; the generator adapts a genre prior only within ±ε; and the transfer
condition "authors should be able to override target curves without
concealing the deviation" is implemented literally — you can leave the
band, but never silently.

**Where:** Figure 2's interactive counterpart; in a talk, drag tension
outside the band and point at the amber handle and the logged edit.

---

## 3. Story graph and node records (center)

**What.** The versioned graph. Green stripe = valid, red = needs review,
amber square = approved placeholder, double edge = ending. A **red dashed
edge labeled "blocked"** carries a diagnostic. A **dashed blue outline** is
the scoped revalidation boundary of the last accepted edit.

**How.**
1. Click any node: the record below shows the full Eq. 4 tuple —
   description, preconditions, add/delete effects, gameplay bindings,
   capability mapping, provenance (append-only), branch state on its
   primary path, and path adherence. Long sections are collapsible ([+]).
2. Click the blocked edge's source node to read the diagnostic (e.g. *"N7
   requires knows(player, sabotage_signature), absent after N6; N5 state is
   branch-local"*).
3. Your click also sets the **expansion point** used by Live generation
   (Sec. 6).

**Rationale.** EdgeValid (Eq. 6) evaluated per edge with branch-local
state — the paper's core example is that a fact on a sibling branch cannot
license a transition. The blocked edge with its diagnostic *is* Figure 4a;
after you approve the repair, the dashed boundary around N6b–N9 while N5
stays untouched *is* Figure 4b (Eq. 13).

**Where:** the before/after repair screenshots; explaining branch-local
state to anyone in one image.

---

## 4. Authorization episodes (right panel)

**What.** The queue of pending proposals, each presented through Figure 3's
five stages: **1** AI proposal (+ validator diagnostic), **2** commitment
profile (reversibility, scope, meaning/reachability/persistence, new label,
implementation consequence, gate result — as chips), **3** evidence package
(branch state, affected region, mapping, provenance, bounded alternatives,
expected revalidation — collapsible), **4** authorization (the bounded
response buttons + Defer), **5** accountable outcome (actor, action,
version, revalidated set).

**How — the canonical walkthrough (hydro brief, episode E1):**
1. Select **E1 Blocked transition N6–N7**.
2. Read stage 1's diagnostic, open stage 3's evidence panels.
3. Press **Approve: insert N6b 'Decode the log'**.
4. Watch: N6b appears in the graph, the red edge turns clean, the dashed
   blue boundary covers exactly N6b–N9, the graph version bumps, and the
   decision lands in the history panel with the revalidated set.

Then try E3 (approve a *visible placeholder* for the unregistered
CrowdSystem — an amber marker appears on N7, and in the demo the station
carries a `[PLACEHOLDER]` sign) and E4 (reject or approve the irreversible
ending deletion).

**Rationale.** This is the paper's contribution — proposal-level
authorization: initiative allocated from the commitments and dependency
consequences of each change (Eq. 14), evidence for authorization rather
than only a diagnosis (Design implication 3), and placeholders as explicit
production decisions rather than low-ranked alternatives.

**Where:** the paper's interface figure; the centerpiece of any demo.

---

## 5. Authorization policy (header: Automatic / Assisted / Strict)

**How.**
1. Default is **Assisted** — at startup it auto-applied E2 (a registered
   label normalization); find it in the history panel marked *Auto*.
2. Switch to **Strict**: nothing is automatic anymore.
3. Switch to **Automatic**: every *gate-valid* pending change applies
   itself — including E4, which silently deletes the N9 ending. Look at
   the graph, then at the log.
4. Press **Undo** to bring the ending back (Sec. 8).

**Rationale.** The RQ2 policy study: Automatic and Assisted apply logged
registered normalizations, Strict reviews everything, and Automatic also
applies other gate-valid changes — which is precisely why it scored lowest
on control and trust (Table 3). Step 3 reproduces that finding as a lived
moment; it happened unscripted during development and is written up in the
paper-observations note.

**Where:** demonstrating Design implication 2 (allocate initiative by
reversibility and scope) in thirty seconds.

---

## 6. Proposal sources: Curated vs Live LLM

**What.** The authorization panel's source switch. **Curated** (default) is
the reproducible built-in queue; **Live** generates real proposals from an
OpenAI-compatible endpoint.

**How (Live).**
1. Copy `LLMConfig.example.ini` → `LLMConfig.ini` at the project root; set
   `Endpoint`, `Model`, and `ApiKey` (empty for local Ollama / LM Studio).
   The file is gitignored — a key never enters the repo.
2. Switch to **Live LLM**; click a node in the graph (the expansion point).
3. **Generate proposal under \<node\>**: three candidates are requested,
   scored by target fit + implementability + predicate licensing
   (simplified Eq. 11), and the best is queued as a normal episode — the
   losers appear inside its evidence as ranked, reviewed alternatives.
4. **Expand frontier**: one round of Algorithm 1 — up to three expansion
   points (shallowest first), three candidates each, best per point
   queued. Repeat to grow the graph, up to a 16-node budget.
5. Authorize the queued episodes exactly like curated ones: an
   unsatisfiable precondition fails the gate with a diagnostic, an
   unregistered capability becomes a placeholder decision, an unlicensed
   predicate is flagged as a new-label commitment.

**Rationale.** The generator cannot touch the graph except through an
authorized episode — the separation of candidate inference from
authorization that the contract model argues for. Curated stays first-class
because live output is unrepeatable: demos, screenshots, and tests need
determinism (and the curated set doubles as test fixtures).

**Where:** live mode for showing the full generate→authorize loop in a
talk; curated for anything that must replay identically.

---

## 7. Save / Load and story briefs

**How (Save/Load).** Header **Save** writes the complete state — graph,
curves, manifest, episodes, decisions — to `Saved/ContractState.json`
(plus a timestamped backup). **Load** restores it; blocked flags and
statuses are *recomputed* by the validator on load, never trusted from
disk. Loading is undoable.

**How (Briefs).** Header **Briefs** opens the world picker:
1. Three built-ins — *Hydro-Station Mystery*, *Dam Breach* (disaster),
   *Derelict Station* (science fiction). Each has its own predicates,
   manifest, genre curves, curated episode, and its own branch-locally
   blocked edge. Switching is undoable.
2. Any `.json` in `<Project>/Briefs/` appears in the list — brief files
   are the same format as Save files.
3. **Save current as brief** writes the working state into `Briefs/`, so
   the authoring loop closes: grow a world with the LLM, curate it, save
   it as a reusable brief.

**Rationale.** The three worlds echo the study's scenario families and
demonstrate that the contract machinery is world-agnostic — the Figure 4
blocking pattern reappears naturally as a jammed-gate fact (disaster) and
an unpowered door (sci-fi). JSON-as-brief keeps authoring inspectable and
diffable (the manuscript's inspectable-history commitment, applied to
content).

**Where:** the generalization argument for reviewers ("same machinery,
three worlds, one click"); preparing task content for any future study.

---

## 8. Undo — rollback as a versioned transformation

**How.** After any decision, an amber **Undo (n)** button appears (its
tooltip names what it will revert). Click it: the state is restored, the
graph version moves **forward** (v19 → v20, never backward), and the log
gains a `Rollback` record *while keeping* the record of the undone
decision. Works for decisions, policy auto-applies, loads, and brief
switches; up to 20 deep, session-scoped.

**Rationale.** Design implication 4: authorization must stay accountable
*after* the decision. An undo that erased history would violate the
append-only provenance the whole system is built on — so rollback is
modeled as one more versioned transformation with a cause and an actor.

**Where:** paired with the Automatic-policy incident (Sec. 5) it makes a
complete story: silent irreversible change → inspectable log → accountable
reversal.

---

## 9. The playable demo

**How.**
1. **Play accepted graph [Tab]** (or Tab). The current brief's *accepted*
   graph becomes a walkable scene — stations per node, per-genre fog and
   lighting, NPCs at character scenes, props matched to bindings.
2. WASD + mouse to move. Walk to the **blue** station, press **E** to
   execute the root.
3. When a node offers several choices, a dialogue prompt opens — press
   **1** or **2**. Only the chosen successor unlocks: a run is one path.
   A choice whose guard is unsatisfied is marked and refused.
4. Try to shortcut: in the hydro brief go locker → log → turbine hall
   *before* approving E1. The station flashes **red**: *runtime gate —
   precondition false on this run*. The engine enforces exactly the gate
   the validator diagnosed at design time.
5. Tab back, approve E1's repair, Tab in again (each entry is a fresh
   clean-state run of the *current* graph) — now the decode station exists
   and the path plays through.
6. Reaching an ending shows the summary card: ending text, executed path,
   facts, and the run's adherence to your approved curves. The full trace
   is in `Saved/RuntimeTrace.json`.

**Rationale.** RQ3's separation of *declared* support from *runtime
conformance*: a graph can satisfy its predicates and still fail at
runtime, so the demo re-checks guards and preconditions against live facts
rather than trusting the authoring-time result. The design-time/runtime
parity in step 4→5 is the paper's argument made playable. Directed
single-path runs mirror RQ4's selected sequences.

**Where:** the demo half of any talk; video figure; the before/after gate
moment is the single most persuasive 20 seconds in the prototype.

---

## 10. Tests — the in-app scenario runner

**How.**
1. Header **Tests** → a panel lists eight end-to-end scenarios; click one
   (or **Run all**). Each shows per-step PASS/FAIL with details (the
   diagnostic found, the exact revalidation boundary, byte counts, facts
   at the ending).
2. The flagship is **Full authoring loop**: frontier → three ranked
   candidates (deterministic mock generator, through the *production*
   ranking/validation code) → authorization → scoped revalidation →
   save/load round-trip → runtime gate parity → replay to an ending.
3. Scenarios run on an **isolated contract copy** — your session state and
   saved files are untouched, so it is safe mid-demo.
4. The same scenarios run headless for CI:
   `UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests
   NarrativeContract; Quit" -unattended -nopause -nullrhi -log`
   (or editor: Tools → Session Frontend → Automation, filter
   "NarrativeContract" — ~15 unit tests plus the scenario suite).

**Rationale.** The interaction model ships with executable verification of
its own claims. This is not hypothetical: the first failure this runner
ever produced was a real nondeterminism bug in frontier ordering, found on
first run on a second machine — evidence that implementation commitments
hide in innocuous places, which is the paper's own thesis.

**Where:** artifact review (a reviewer can verify the core claims in one
click); regression safety for any future code change; a slide about the
artifact itself.

---

## 11. Figure mode — making paper figures

**How.**
1. **Light theme** for a white "paper" palette (Dark theme to return).
2. Stage the state you want: select E1 for the five-stage episode view, or
   approve it to show the revalidation boundary, or open the demo.
3. **F10** (capture mode) strips every legend, hint, and utility button.
4. **F9** writes a 2×-resolution PNG to `Saved/Screenshots/`.
5. F10 again to restore the chrome.

**Where:** every figure of the graphical prototype in the paper; matching
light-background stills for slides.

---

## 12. Telemetry — session logs for analysis

**What.** Every session continuously writes
`Saved/SessionTelemetry.json`: a raw event stream with session-relative
timestamps — `episode_selected`, `evidence_panel_opened` (which evidence
view, for which episode), `node_selected`, `decision`, `policy_changed`,
`generate_requested`, `choice_selected`, `save`/`load`, `undo`,
`brief_loaded`, `demo_entered`/`demo_exited`, `node_executed`,
`runtime_blocked`. The history panel shows a live count. Alongside it:
`DecisionLog.json` (every authorization with actor, policy, versions,
revalidated set) and `RuntimeTrace.json` (per-run execution evidence).

**Rationale.** Mirrors the RQ2 logging method: raw events, segmented into
authorization episodes offline. Panel-opening events in particular are the
evidence-use measure from the evidence-presentation study.

**Where:** if you ever run participants on the graphical version, the
logging half of the method section already exists; also handy for
instrumenting your own demo rehearsals.

---

## Three suggested end-to-end routes

**The 5-minute talk demo.** Hydro brief → show curves, drag one handle out
of band → click the blocked edge, read the diagnostic → E1: evidence →
approve → boundary flash → Tab into the demo, get gated at the turbine
hall, Tab out, Tab in, play to an ending → ending card.

**The reviewer route.** Tests → Run all (8/8) → Briefs → Dam Breach →
same blocked-edge pattern in a new world → headless automation command
from Sec. 10.

**The generative route.** LLMConfig.ini → Live → Expand frontier twice →
authorize what survived the gate → Save current as brief → Tab in and play
the world that did not exist ten minutes ago.
