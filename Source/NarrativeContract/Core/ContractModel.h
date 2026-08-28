// The active Narrative Capability Contract and versioned story graph.
// Owns validation (Eq. 6), routing (policy Pi), decision application,
// scoped revalidation (Eq. 13), and the append-only decision log.

#pragma once

#include "CoreMinimal.h"
#include "ContractTypes.h"

DECLARE_MULTICAST_DELEGATE(FOnContractModelChanged);

class FContractModel
{
public:
	FContractModel();

	// --- Data -------------------------------------------------------------
	TArray<FStoryNode> Nodes;
	FTargetCurve Curves[NumAxes];
	TArray<FCapabilityRecord> Manifest;
	TArray<FAuthorizationEpisode> Episodes;
	TArray<FDecisionRecord> DecisionLog;
	TArray<FCurveEditRecord> CurveEdits;
	TArray<FString> ImplementationNeeds;
	FContractVersions Versions;
	EAuthorizationPolicy Policy = EAuthorizationPolicy::Assisted;
	FString StoryTitle;
	FString GenreLabel;

	// --- Proposal source (v3) --------------------------------------------
	EProposalSourceMode ProposalMode = EProposalSourceMode::Curated;
	FString LiveStatus;              // shown in the authorization panel
	FString SelectedNodeId;          // graph selection; expansion point for Live
	int32 LiveEpisodeCounter = 0;

	// Bound by the game instance; issues the async LLM request for the
	// given expansion node. The model stays free of HTTP concerns.
	TFunction<void(const FString& /*NodeId*/)> LiveProposalRequestHandler;

	// Bound by the game instance; runs one frontier-expansion round
	// (Algorithm 1: several expansion points, candidates per point).
	TFunction<void()> FrontierExpansionHandler;

	// Hard cap on graph size for live growth (the evaluated prototype used
	// a 12-node budget; a little headroom here).
	static constexpr int32 LiveNodeBudget = 16;

	// Node ids revalidated by the most recent accepted edit (UI highlight).
	TArray<FString> LastRevalidationBoundary;

	// --- Undo (v6): session-scoped, never serialized -----------------------
	TArray<FUndoEntry> UndoStack;
	static constexpr int32 MaxUndoDepth = 20;

	// --- Telemetry (v6): raw RQ2-style event stream ------------------------
	TArray<FTelemetryEvent> Telemetry;
	double SessionStartSeconds = 0.0;

	FOnContractModelChanged OnChanged;

	// --- Setup ------------------------------------------------------------
	// Empties every content collection and restores defaults; called by
	// every brief builder before it populates the model.
	void ResetContent();

	// Figure 4 hydro-station mystery. bAutoApplyPolicy=false leaves the full
	// episode queue pending (used by the automation tests to exercise
	// routing under each policy from a known state).
	void BuildSampleData(bool bAutoApplyPolicy = true);

	// --- Graph access -----------------------------------------------------
	FStoryNode* FindNode(const FString& NodeId);
	const FStoryNode* FindNode(const FString& NodeId) const;
	FStoryChoice* FindChoice(const FString& NodeId, const FString& ChoiceId);
	FString RootNodeId() const { return Nodes.Num() > 0 ? Nodes[0].NodeId : FString(); }

	// Branch-local state S_t after applying root..node effects along the
	// primary incoming path (node effects delta_V, Eq. 5a).
	TSet<FString> ComputeBranchStateAfterNode(const FString& NodeId) const;

	// Children reachable through choices (live edges only).
	TArray<FString> ChildNodeIds(const FString& NodeId) const;

	// Descendants including the node itself (dependency closure used for
	// the scoped-revalidation reach, Eq. 13).
	TArray<FString> ReachableFrom(const FString& NodeId) const;

	// --- Validation -------------------------------------------------------
	// Re-runs EdgeValid (Eq. 6) for every edge whose source is in the given
	// set (empty set => whole graph) and refreshes node/edge statuses.
	void Revalidate(const TArray<FString>& ScopeNodeIds);

	// --- Curves -----------------------------------------------------------
	void SetApprovedCurveValue(ENarrativeAxis Axis, int32 ControlIndex, float NewValue, const FString& Actor);

	// Path adherence Adh(P) (Eq. 12) along the primary spine to a node.
	float ComputePathAdherence(const FString& LeafNodeId) const;

	// --- Authorization ----------------------------------------------------
	FAuthorizationEpisode* FindEpisode(const FString& EpisodeId);

	// Routes every unresolved episode under the current policy and applies
	// the ones the policy licenses automatically (logged, inspectable).
	void SetPolicy(EAuthorizationPolicy NewPolicy, const FString& Actor);
	void RouteAndAutoApply(const FString& Actor);
	EPolicyRoute RouteEpisode(const FAuthorizationEpisode& Episode) const;

	// A recorded human (or policy) action on an episode.
	void ResolveEpisode(const FString& EpisodeId, const FString& OptionId, EProposalAction Action, const FString& Actor);

	int32 NumPendingEpisodes() const;

	// Admits a freshly generated (Live) episode into the queue: routes it
	// under the current policy, auto-applies if licensed, and notifies.
	void AddEpisode(FAuthorizationEpisode Episode);

	// Every predicate the current graph licenses (preconditions, effects,
	// guards). Live proposals introducing predicates outside this set are
	// flagged as new-label commitments.
	TSet<FString> CollectLicensedPredicates() const;

	// Request a live proposal at SelectedNodeId through the bound handler.
	void RequestLiveProposal();

	// One frontier round through the bound handler (Live mode only).
	void RequestFrontierExpansion();

	// Expandable frontier: live non-ending nodes with spare choice slots,
	// oldest (shallowest) first, as in Algorithm 1's breadth-first frontier.
	TArray<FString> FrontierNodeIds(int32 MaxChoicesPerNode = 2) const;

	// Live (not removed/proposed) node count, for the budget check.
	int32 NumLiveNodes() const;

	// --- Undo -------------------------------------------------------------
	// Pushes the complete current state; called automatically before every
	// decision, and manually before destructive operations (e.g. Load).
	void PushUndoSnapshot(const FString& EpisodeId, const FString& Label);

	// Restores the most recent snapshot as a NEW versioned transformation:
	// the decision log is preserved and gains a Rollback record, and the
	// graph version increases monotonically (never rewinds).
	void UndoLastDecision(const FString& Actor);

	bool CanUndo() const { return UndoStack.Num() > 0; }
	FString LastUndoLabel() const { return UndoStack.Num() > 0 ? UndoStack.Last().Label : FString(); }

	// --- Telemetry --------------------------------------------------------
	// Appends one raw event and exports the stream. Deliberately does NOT
	// broadcast OnChanged, so logging from UI callbacks cannot trigger
	// rebuild loops.
	void LogEvent(const FString& Type, const FString& Target, const FString& Detail = FString());
	void ExportTelemetryJson() const; // Saved/SessionTelemetry.json

	// --- Persistence ------------------------------------------------------
	void ExportDecisionLogJson() const; // Saved/DecisionLog.json

	static FString NowTimestamp();

private:
	void ApplyMutations(const TArray<FGraphMutation>& Mutations, FDecisionRecord& Record);
	void AppendDecision(FDecisionRecord Record);
	void Notify() { OnChanged.Broadcast(); }
};
