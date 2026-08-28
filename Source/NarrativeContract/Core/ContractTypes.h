// Core data types for the Narrative Capability Contract prototype.
// Mirrors the formalism of the paper: nodes/choices with add-delete effects
// (Eq. 4-6), five-axis target curves with a bounded prior (Eq. 2-3),
// capability grounding (Eq. 8-9), authorization episodes (Eq. 14),
// and scoped revalidation (Eq. 13).

#pragma once

#include "CoreMinimal.h"

// ---------------------------------------------------------------------------
// Axes (Table 1)
// ---------------------------------------------------------------------------

enum class ENarrativeAxis : uint8
{
	Valence = 0,
	Tension,
	Agency,
	Information,
	Stakes,
	Count
};

FORCEINLINE const TCHAR* AxisDisplayName(ENarrativeAxis Axis)
{
	switch (Axis)
	{
	case ENarrativeAxis::Valence:     return TEXT("Valence");
	case ENarrativeAxis::Tension:     return TEXT("Tension");
	case ENarrativeAxis::Agency:      return TEXT("Agency");
	case ENarrativeAxis::Information: return TEXT("Information");
	case ENarrativeAxis::Stakes:      return TEXT("Stakes");
	default:                          return TEXT("Unknown");
	}
}

static constexpr int32 NumAxes = 5;
static constexpr int32 NumCurveControlPoints = 5; // x = 0, .25, .50, .75, 1

// ---------------------------------------------------------------------------
// Target curves (Sec. 3.2)
// ---------------------------------------------------------------------------

struct FTargetCurve
{
	ENarrativeAxis Axis = ENarrativeAxis::Valence;

	// Genre prior mu_a at the five control positions.
	float Prior[NumCurveControlPoints] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

	// Bounded model proposal (Eq. 3), clip(mu + clip(delta, -eps, eps), 0, 1).
	float Proposal[NumCurveControlPoints] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

	// Designer-approved curve gamma*_a used during planning.
	float Approved[NumCurveControlPoints] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};

	// Permitted adaptation bound epsilon_a (the evaluated prototype used .20).
	float Epsilon = 0.20f;

	static float ControlX(int32 Index)
	{
		return static_cast<float>(Index) / static_cast<float>(NumCurveControlPoints - 1);
	}

	static float EvalPoints(const float* Points, float X)
	{
		X = FMath::Clamp(X, 0.f, 1.f);
		const float Scaled = X * (NumCurveControlPoints - 1);
		const int32 I0 = FMath::Clamp(FMath::FloorToInt32(Scaled), 0, NumCurveControlPoints - 2);
		const float Frac = Scaled - static_cast<float>(I0);
		return FMath::Lerp(Points[I0], Points[I0 + 1], Frac);
	}

	float EvalApproved(float X) const { return EvalPoints(Approved, X); }
	float EvalPrior(float X) const { return EvalPoints(Prior, X); }

	bool IsHandleOutsideBand(int32 Index) const
	{
		return FMath::Abs(Approved[Index] - Prior[Index]) > Epsilon + KINDA_SMALL_NUMBER;
	}
};

// A logged edit to an approved curve handle (actor, old/new, rationale).
struct FCurveEditRecord
{
	ENarrativeAxis Axis = ENarrativeAxis::Valence;
	int32 ControlIndex = 0;
	float OldValue = 0.f;
	float NewValue = 0.f;
	FString Actor;
	FString Timestamp;
};

// ---------------------------------------------------------------------------
// Grounding and capabilities (Sec. 3.4)
// ---------------------------------------------------------------------------

enum class EGroundingStatus : uint8
{
	NotRequired,          // node requests no capabilities
	Implemented,          // complete compatible mapping exists in the manifest
	ApprovedPlaceholder,  // explicitly approved by an authorized actor
	Unresolved            // requested capability has no licensed mapping
};

FORCEINLINE const TCHAR* GroundingDisplayName(EGroundingStatus S)
{
	switch (S)
	{
	case EGroundingStatus::NotRequired:         return TEXT("No capability required");
	case EGroundingStatus::Implemented:         return TEXT("Implemented (manifest mapping)");
	case EGroundingStatus::ApprovedPlaceholder: return TEXT("Approved placeholder");
	case EGroundingStatus::Unresolved:          return TEXT("Unresolved");
	default:                                    return TEXT("Unknown");
	}
}

// One entry of the project-authored Engine Capability Manifest.
struct FCapabilityRecord
{
	FString CapabilityId;          // abstract capability identifier
	FString ImplementationClass;   // registered project class
	FString EntityTypes;           // accepted entity types
	FString Parameters;            // parameter bounds
	FString EvidenceFields;        // expected runtime evidence
	bool bRegistered = true;       // false => not licensed by this project
};

// ---------------------------------------------------------------------------
// Story graph (Sec. 3.3): nodes, choices, add-delete effects
// ---------------------------------------------------------------------------

enum class ENodeStatus : uint8
{
	Valid,
	NeedsReview,
	Unsupported,
	Proposed,   // pending candidate, not yet admitted to the versioned graph
	Removed
};

FORCEINLINE const TCHAR* NodeStatusDisplayName(ENodeStatus S)
{
	switch (S)
	{
	case ENodeStatus::Valid:       return TEXT("Valid");
	case ENodeStatus::NeedsReview: return TEXT("NeedsReview");
	case ENodeStatus::Unsupported: return TEXT("Unsupported");
	case ENodeStatus::Proposed:    return TEXT("Proposed");
	case ENodeStatus::Removed:     return TEXT("Removed");
	default:                       return TEXT("Unknown");
	}
}

// A choice chi = (label, guard, effects) leading to a successor node.
struct FStoryChoice
{
	FString ChoiceId;
	FString Label;
	TArray<FString> Guard;       // predicates required after node effects
	TArray<FString> AddEffects;  // q+_chi
	TArray<FString> DelEffects;  // q-_chi
	FString TargetNodeId;        // successor
	bool bBlocked = false;       // failed EdgeValid at last validation
	FString BlockDiagnostic;     // human-readable reason when blocked
};

// A node v = (d, e, p, q+, q-, c, r, b, K, rho) per Eq. 4.
struct FStoryNode
{
	FString NodeId;
	FString Title;
	FString Description;                 // event description d
	TArray<FString> Entities;            // grounded entities e
	TArray<FString> Preconditions;       // p
	TArray<FString> AddEffects;          // q+
	TArray<FString> DelEffects;          // q-
	TArray<FStoryChoice> Choices;        // c
	float AxisEstimates[NumAxes] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f}; // r (Eq. 7)
	TArray<FString> GameplayBindings;    // b: intended play
	TArray<FString> RequiredCapabilities; // K
	TMap<FString, FString> SelectedMapping; // capability -> implementation
	TArray<FString> Provenance;          // rho, append-only
	EGroundingStatus Grounding = EGroundingStatus::NotRequired;
	ENodeStatus Status = ENodeStatus::Valid;
	bool bEnding = false;

	// Primary incoming edge used to derive branch-local state
	// ("the state passed to one successor is derived only from its
	//   incoming path").
	FString PrimaryParentId;
	FString PrimaryIncomingChoiceId;

	// Layout hints for the graph panel (grid coordinates).
	int32 Depth = 0;
	int32 Lane = 0;

	bool HasCapabilities() const { return RequiredCapabilities.Num() > 0; }
};

// ---------------------------------------------------------------------------
// Authorization episodes (Sec. 3.6, Eq. 14)
// ---------------------------------------------------------------------------

enum class EAuthorizationPolicy : uint8
{
	Automatic,
	Assisted,
	Strict
};

FORCEINLINE const TCHAR* PolicyDisplayName(EAuthorizationPolicy P)
{
	switch (P)
	{
	case EAuthorizationPolicy::Automatic: return TEXT("Automatic");
	case EAuthorizationPolicy::Assisted:  return TEXT("Assisted");
	case EAuthorizationPolicy::Strict:    return TEXT("Strict");
	default:                              return TEXT("Unknown");
	}
}

enum class EPolicyRoute : uint8
{
	Auto,     // predefined reversible correction may proceed, logged
	Review,   // requires a recorded human action
	Applied,  // already resolved
	Deferred
};

enum class EReversibility : uint8 { Reversible, Irreversible };
enum class EDependencyScope : uint8 { Local, Branch, Global };

enum class EImplConsequence : uint8
{
	None,
	RegisteredMapping,   // grounded in the manifest
	Placeholder,         // requires an explicitly approved placeholder
	NewRequirement       // introduces an engine obligation the project lacks
};

FORCEINLINE const TCHAR* ImplConsequenceDisplayName(EImplConsequence C)
{
	switch (C)
	{
	case EImplConsequence::None:              return TEXT("None");
	case EImplConsequence::RegisteredMapping: return TEXT("Registered mapping");
	case EImplConsequence::Placeholder:       return TEXT("Placeholder required");
	case EImplConsequence::NewRequirement:    return TEXT("New engine requirement");
	default:                                  return TEXT("Unknown");
	}
}

// kappa_j: the commitment profile of a proposal.
struct FCommitmentProfile
{
	EReversibility Reversibility = EReversibility::Reversible;
	EDependencyScope Scope = EDependencyScope::Local;
	bool bChangesNarrativeMeaning = false;
	bool bChangesReachability = false;
	bool bChangesPersistentState = false;
	bool bIntroducesNewLabel = false;
	EImplConsequence ImplConsequence = EImplConsequence::None;

	bool IsRegisteredLocalNormalization() const
	{
		return Reversibility == EReversibility::Reversible
			&& Scope == EDependencyScope::Local
			&& !bChangesNarrativeMeaning
			&& !bChangesReachability
			&& !bChangesPersistentState
			&& !bIntroducesNewLabel
			&& (ImplConsequence == EImplConsequence::None
				|| ImplConsequence == EImplConsequence::RegisteredMapping);
	}
};

// Where candidate proposals come from.
enum class EProposalSourceMode : uint8
{
	Curated, // the reproducible sample queue (also used as test fixtures)
	Live     // generated on demand by an OpenAI-compatible endpoint
};

// Graph mutations an episode option can carry (kept data-driven so the
// authorization machinery is generic rather than special-cased).
enum class EGraphMutationType : uint8
{
	InsertNodeBetween,    // insert NewNode between TargetNodeId's primary parent and TargetNodeId
	AppendNodeUnder,      // add NewNode as a new child of TargetNodeId (choice label = StringParamA)
	RemovePrecondition,   // remove StringParamA from TargetNodeId's preconditions
	SetChoiceLabel,       // on TargetNodeId, choice StringParamA gets label StringParamB
	SetGrounding,         // TargetNodeId's grounding becomes Grounding
	AddCapabilityNeed,    // append an implementation-need record (no graph change)
	RemoveNode,           // TargetNodeId marked Removed
	AddProvenance         // append StringParamA to TargetNodeId's provenance
};

struct FGraphMutation
{
	EGraphMutationType Type = EGraphMutationType::AddProvenance;
	FString TargetNodeId;
	FString StringParamA;
	FString StringParamB;
	EGroundingStatus Grounding = EGroundingStatus::NotRequired;
	FStoryNode NewNode; // used by InsertNodeBetween
};

// d_j: the recorded action kinds. Rollback is appended (v6): undoing a
// decision is itself a versioned, recorded transformation, never an
// erasure of history.
enum class EProposalAction : uint8
{
	None,
	AutoApplied,
	Approve,
	Edit,
	Substitute,
	Defer,
	Reject,
	Rollback
};

FORCEINLINE const TCHAR* ActionDisplayName(EProposalAction A)
{
	switch (A)
	{
	case EProposalAction::AutoApplied: return TEXT("Auto");
	case EProposalAction::Approve:     return TEXT("Approve");
	case EProposalAction::Edit:        return TEXT("Edit");
	case EProposalAction::Substitute:  return TEXT("Substitute");
	case EProposalAction::Defer:       return TEXT("Defer");
	case EProposalAction::Reject:      return TEXT("Reject");
	case EProposalAction::Rollback:    return TEXT("Rollback");
	default:                           return TEXT("None");
	}
}

// A bounded response offered by the validator for this episode.
struct FEpisodeOption
{
	FString OptionId;
	FString Label;              // button label
	FString Description;        // what this response does
	EProposalAction ActionKind = EProposalAction::Approve;
	TArray<FGraphMutation> Mutations;
	bool bRecommended = false;
};

// E_j: the evidence set exposed for authorization.
struct FEvidencePackage
{
	TArray<FString> BranchState;      // S_t at the affected location
	TArray<FString> AffectedRegion;   // node ids inside the dependency closure
	TArray<FString> MappingLines;     // capability -> implementation, params
	TArray<FString> ProvenanceLines;  // model/version/actor chain
	TArray<FString> AlternativeLines; // short description of the bounded alternatives
	TArray<FString> RevalidationChecks; // expected checks if accepted
};

// A_j = (v_j, kappa_j, zeta_j, E_j, d_j, R_j) per Eq. 14.
struct FAuthorizationEpisode
{
	FString EpisodeId;
	FString Title;
	FString ProposalClass;     // e.g. "Repair", "Normalization", "Placeholder", "Deletion"
	FString ProposalText;      // the generated change being proposed
	FString Diagnostic;        // validator diagnosis (e.g. failed precondition)
	FString TargetNodeId;
	FCommitmentProfile Profile;
	FEvidencePackage Evidence;
	TArray<FEpisodeOption> Options;
	bool bGateValid = true;    // H(v, S_t) before authorization

	// Resolution state
	EPolicyRoute Route = EPolicyRoute::Review;
	EProposalAction ResolvedAction = EProposalAction::None;
	FString ResolvedOptionId;
	FString ResolvedBy;
	int32 GraphVersionAfter = -1;
};

// ---------------------------------------------------------------------------
// Decisions, versions, log (Design implication 4: inspectable history)
// ---------------------------------------------------------------------------

struct FDecisionRecord
{
	FString EpisodeId;
	FString EpisodeTitle;
	EProposalAction Action = EProposalAction::None;
	FString OptionLabel;
	FString Actor;
	EAuthorizationPolicy PolicyAtDecision = EAuthorizationPolicy::Assisted;
	int32 GraphVersionBefore = 0;
	int32 GraphVersionAfter = 0;
	TArray<FString> RevalidatedNodes; // R(Delta, G), Eq. 13
	FString Timestamp;
};

// One entry of the session undo stack: the complete serialized state as it
// was immediately before a decision was applied.
struct FUndoEntry
{
	FString EpisodeId;
	FString Label;        // e.g. "Remove ending N9 'Track ally' (Auto)"
	FString SnapshotJson; // full state via ContractSerialization
	FString Timestamp;
};

// One raw interaction event (RQ2-style logging). Segmentation into
// authorization episodes happens offline, as in the study.
struct FTelemetryEvent
{
	FString Type;    // episode_selected, evidence_panel_opened, decision, ...
	FString Target;  // episode/node/panel identifier
	FString Detail;
	double SessionSeconds = 0.0;
	FString Timestamp;
};

// Versioned contract artifacts C_t (Eq. 1).
struct FContractVersions
{
	int32 CoreNarrativeLibrary = 3;   // L
	int32 DomainNarrativeProfile = 5; // D
	int32 EngineCapabilityManifest = 7; // M
	int32 StoryBible = 2;             // B
	int32 GraphVersion = 17;          // matches Figure 4's "graph v17"
};
