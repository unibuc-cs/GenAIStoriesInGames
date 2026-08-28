// Live proposal pipeline, network-free part: prompt construction from the
// active contract, strict-JSON parsing of the model reply, and the
// extractor/validator that turns a parsed proposal into a routed
// authorization episode with real evidence. The HTTP call itself lives in
// UContractGameInstance so this half stays unit-testable.

#pragma once

#include "CoreMinimal.h"
#include "ContractTypes.h"

class FContractModel;

// A parsed candidate successor node as proposed by the language model.
struct FLlmProposal
{
	FString Title;
	FString Description;
	FString ChoiceLabel;               // label of the edge from the expansion node
	TArray<FString> Preconditions;
	TArray<FString> AddEffects;
	TArray<FString> DelEffects;
	TArray<FString> RequiredCapabilities;
	float AxisEstimates[NumAxes] = {0.5f, 0.5f, 0.5f, 0.5f, 0.5f};
	FString Rationale;                 // free text; goes into provenance
	bool bEnding = false;
};

namespace LlmProposals
{
	// Contract-grounded prompts (bounded generation: licensed predicates,
	// manifest capabilities, branch state, target-curve values at the
	// expansion position are all supplied explicitly).
	FString BuildSystemPrompt(const FContractModel& Model);
	FString BuildUserPrompt(const FContractModel& Model, const FString& ExpansionNodeId);

	// Parses the assistant message content (tolerates ```json fences).
	// Returns false with OutError set on malformed replies.
	bool ParseProposalJson(const FString& Content, FLlmProposal& OutProposal, FString& OutError);

	// Multi-candidate form (Algorithm 1: several fresh successors per
	// expansion): accepts {"candidates":[...]} or a single bare object.
	bool ParseCandidatesJson(const FString& Content, TArray<FLlmProposal>& OutProposals, FString& OutError);

	// Candidate ranking, a simplified Eq. 11: target fit against the
	// approved curves at the successor position, weighted with
	// implementability (registered capabilities) and predicate licensing.
	float ScoreProposal(const FContractModel& Model, const FString& ExpansionNodeId, const FLlmProposal& Proposal);

	// The constrained extractor + validators: schema/domain/state/policy
	// checks produce the commitment profile, evidence package, diagnostic,
	// and bounded options. Does NOT mutate the graph. RankedAlternatives
	// (optional) lists the scored sibling candidates for the evidence view.
	FAuthorizationEpisode BuildEpisode(FContractModel& Model, const FString& ExpansionNodeId, const FLlmProposal& Proposal,
		const TArray<FString>& RankedAlternatives = TArray<FString>());

	// Ranks candidates (ScoreProposal), builds the episode for the best one
	// with the losers recorded as scored alternatives. Shared by the real
	// HTTP path and the mock generator in the scenario runner, so both
	// exercise the same code.
	FAuthorizationEpisode RankAndBuildEpisode(FContractModel& Model, const FString& ExpansionNodeId,
		const TArray<FLlmProposal>& Candidates, int32& OutBestIndex, float& OutBestScore);
}
