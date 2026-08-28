// Owns the contract model for the whole session (authoring UI + demo) and
// the network half of the Live proposal source: an OpenAI-compatible
// chat-completions call configured by LLMConfig.ini at the project root
// (untracked; see LLMConfig.example.ini).

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Interfaces/IHttpRequest.h"
#include "../Core/ContractModel.h"
#include "ContractGameInstance.generated.h"

UCLASS()
class NARRATIVECONTRACT_API UContractGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	FContractModel* GetModel() { return Model.Get(); }

private:
	TUniquePtr<FContractModel> Model;

	// LLM configuration (loaded lazily from LLMConfig.ini)
	bool bLlmConfigLoaded = false;
	FString LlmEndpoint;
	FString LlmApiKey;
	FString LlmModelName;
	float LlmTemperature = 0.7f;
	bool bLlmJsonResponseFormat = true;
	bool bLlmRequestInFlight = false;

	// Frontier expansion state (sequential requests, one per expansion node)
	TArray<FString> PendingFrontier;
	int32 FrontierTotal = 0;
	int32 FrontierQueuedEpisodes = 0;

	bool LoadLlmConfig(FString& OutProblem);
	void RequestLiveProposal(const FString& ExpansionNodeId);
	void RequestFrontierExpansion();
	void IssueLlmRequest(const FString& ExpansionNodeId);
	void ContinueFrontier();
	void OnLlmResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FString ExpansionNodeId);
	void SetLiveStatus(const FString& Status);
};
