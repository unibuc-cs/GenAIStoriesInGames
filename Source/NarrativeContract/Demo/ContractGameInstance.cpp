#include "ContractGameInstance.h"

#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "../Core/LlmProposals.h"

void UContractGameInstance::Init()
{
	Super::Init();
	Model = MakeUnique<FContractModel>();
	Model->BuildSampleData();

	// Bind the network half of the Live proposal source.
	TWeakObjectPtr<UContractGameInstance> WeakThis(this);
	Model->LiveProposalRequestHandler = [WeakThis](const FString& NodeId)
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RequestLiveProposal(NodeId);
		}
	};
	Model->FrontierExpansionHandler = [WeakThis]()
	{
		if (WeakThis.IsValid())
		{
			WeakThis->RequestFrontierExpansion();
		}
	};
}

void UContractGameInstance::SetLiveStatus(const FString& Status)
{
	if (Model)
	{
		Model->LiveStatus = Status;
		Model->OnChanged.Broadcast();
	}
}

bool UContractGameInstance::LoadLlmConfig(FString& OutProblem)
{
	if (bLlmConfigLoaded)
	{
		return true;
	}

	const FString ConfigPath = FPaths::ProjectDir() / TEXT("LLMConfig.ini");
	if (!FPaths::FileExists(ConfigPath))
	{
		OutProblem = TEXT("LLMConfig.ini not found at the project root (copy LLMConfig.example.ini).");
		return false;
	}

	FConfigFile File;
	File.Read(ConfigPath);
	File.GetString(TEXT("LLM"), TEXT("Endpoint"), LlmEndpoint);
	File.GetString(TEXT("LLM"), TEXT("ApiKey"), LlmApiKey);
	File.GetString(TEXT("LLM"), TEXT("Model"), LlmModelName);
	// Parsed from strings for broad engine-version compatibility.
	FString TempStr;
	if (File.GetString(TEXT("LLM"), TEXT("Temperature"), TempStr) && !TempStr.IsEmpty())
	{
		LlmTemperature = FCString::Atof(*TempStr);
	}
	FString JsonStr;
	if (File.GetString(TEXT("LLM"), TEXT("JsonResponseFormat"), JsonStr) && !JsonStr.IsEmpty())
	{
		bLlmJsonResponseFormat = JsonStr.ToBool();
	}

	if (LlmEndpoint.IsEmpty())
	{
		LlmEndpoint = TEXT("https://api.openai.com/v1/chat/completions");
	}
	if (LlmModelName.IsEmpty())
	{
		OutProblem = TEXT("LLMConfig.ini: [LLM] Model is not set.");
		return false;
	}
	// Local servers (Ollama, LM Studio) work without a key.

	bLlmConfigLoaded = true;
	return true;
}

void UContractGameInstance::RequestLiveProposal(const FString& ExpansionNodeId)
{
	if (!Model)
	{
		return;
	}
	if (bLlmRequestInFlight)
	{
		SetLiveStatus(TEXT("A generation request is already running."));
		return;
	}
	PendingFrontier.Empty();
	FrontierTotal = 0;
	IssueLlmRequest(ExpansionNodeId);
}

void UContractGameInstance::RequestFrontierExpansion()
{
	if (!Model)
	{
		return;
	}
	if (bLlmRequestInFlight)
	{
		SetLiveStatus(TEXT("A generation request is already running."));
		return;
	}
	if (Model->NumLiveNodes() >= FContractModel::LiveNodeBudget)
	{
		SetLiveStatus(FString::Printf(TEXT("Node budget reached (%d): resolve or prune before expanding."),
			FContractModel::LiveNodeBudget));
		return;
	}

	// Up to three expansion points per round, oldest paths first.
	PendingFrontier = Model->FrontierNodeIds();
	if (PendingFrontier.Num() > 3)
	{
		PendingFrontier.SetNum(3);
	}
	if (PendingFrontier.Num() == 0)
	{
		SetLiveStatus(TEXT("No expandable frontier: every live node is an ending or has a full choice set."));
		return;
	}
	FrontierTotal = PendingFrontier.Num();
	FrontierQueuedEpisodes = 0;

	const FString First = PendingFrontier[0];
	PendingFrontier.RemoveAt(0);
	IssueLlmRequest(First);
}

void UContractGameInstance::ContinueFrontier()
{
	if (PendingFrontier.Num() == 0)
	{
		if (FrontierTotal > 0)
		{
			SetLiveStatus(FString::Printf(TEXT("Frontier round complete: %d expansion point(s), %d episode(s) queued for authorization."),
				FrontierTotal, FrontierQueuedEpisodes));
			FrontierTotal = 0;
		}
		return;
	}
	const FString Next = PendingFrontier[0];
	PendingFrontier.RemoveAt(0);
	IssueLlmRequest(Next);
}

void UContractGameInstance::IssueLlmRequest(const FString& ExpansionNodeId)
{
	FString Problem;
	if (!LoadLlmConfig(Problem))
	{
		PendingFrontier.Empty();
		SetLiveStatus(Problem);
		return;
	}
	if (!Model->FindNode(ExpansionNodeId))
	{
		SetLiveStatus(FString::Printf(TEXT("Expansion node %s not found."), *ExpansionNodeId));
		ContinueFrontier();
		return;
	}

	// Build the chat request.
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("model"), LlmModelName);
	Root->SetNumberField(TEXT("temperature"), LlmTemperature);
	if (bLlmJsonResponseFormat)
	{
		TSharedRef<FJsonObject> Format = MakeShared<FJsonObject>();
		Format->SetStringField(TEXT("type"), TEXT("json_object"));
		Root->SetObjectField(TEXT("response_format"), Format);
	}
	TArray<TSharedPtr<FJsonValue>> Messages;
	{
		TSharedRef<FJsonObject> Sys = MakeShared<FJsonObject>();
		Sys->SetStringField(TEXT("role"), TEXT("system"));
		Sys->SetStringField(TEXT("content"), LlmProposals::BuildSystemPrompt(*Model));
		Messages.Add(MakeShared<FJsonValueObject>(Sys));
		TSharedRef<FJsonObject> Usr = MakeShared<FJsonObject>();
		Usr->SetStringField(TEXT("role"), TEXT("user"));
		Usr->SetStringField(TEXT("content"), LlmProposals::BuildUserPrompt(*Model, ExpansionNodeId));
		Messages.Add(MakeShared<FJsonValueObject>(Usr));
	}
	Root->SetArrayField(TEXT("messages"), Messages);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(LlmEndpoint);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	if (!LlmApiKey.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *LlmApiKey));
	}
	Request->SetTimeout(60.f);
	Request->SetContentAsString(Body);

	TWeakObjectPtr<UContractGameInstance> WeakThis(this);
	Request->OnProcessRequestComplete().BindLambda(
		[WeakThis, ExpansionNodeId](FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk)
		{
			if (WeakThis.IsValid())
			{
				WeakThis->OnLlmResponse(Req, Resp, bOk, ExpansionNodeId);
			}
		});

	bLlmRequestInFlight = true;
	if (FrontierTotal > 0)
	{
		SetLiveStatus(FString::Printf(TEXT("Expanding frontier %d/%d: generating candidates under %s (%s)..."),
			FrontierTotal - PendingFrontier.Num(), FrontierTotal, *ExpansionNodeId, *LlmModelName));
	}
	else
	{
		SetLiveStatus(FString::Printf(TEXT("Generating candidates under %s (%s)..."), *ExpansionNodeId, *LlmModelName));
	}
	Request->ProcessRequest();
}

void UContractGameInstance::OnLlmResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully, FString ExpansionNodeId)
{
	bLlmRequestInFlight = false;
	if (!Model)
	{
		return;
	}

	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		SetLiveStatus(TEXT("LLM request failed: no connection to the endpoint."));
		ContinueFrontier();
		return;
	}
	if (Response->GetResponseCode() < 200 || Response->GetResponseCode() >= 300)
	{
		SetLiveStatus(FString::Printf(TEXT("LLM request failed: HTTP %d -- %s"),
			Response->GetResponseCode(), *Response->GetContentAsString().Left(180)));
		ContinueFrontier();
		return;
	}

	// Extract choices[0].message.content.
	FString Content;
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
		{
			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (Root->TryGetArrayField(TEXT("choices"), Choices) && Choices && Choices->Num() > 0)
			{
				const TSharedPtr<FJsonObject>* First = nullptr;
				if ((*Choices)[0]->TryGetObject(First) && First && First->IsValid())
				{
					const TSharedPtr<FJsonObject>* Message = nullptr;
					if ((*First)->TryGetObjectField(TEXT("message"), Message) && Message && Message->IsValid())
					{
						(*Message)->TryGetStringField(TEXT("content"), Content);
					}
				}
			}
		}
	}
	if (Content.IsEmpty())
	{
		SetLiveStatus(TEXT("LLM reply had no message content (unexpected response shape)."));
		ContinueFrontier();
		return;
	}

	TArray<FLlmProposal> Candidates;
	FString ParseError;
	if (!LlmProposals::ParseCandidatesJson(Content, Candidates, ParseError))
	{
		SetLiveStatus(FString::Printf(TEXT("Proposal rejected by the extractor: %s."), *ParseError));
		ContinueFrontier();
		return;
	}

	// Rank candidates by target fit + implementability (Eq. 11, simplified)
	// and queue the best; the rest are recorded as reviewed alternatives.
	int32 BestIndex = 0;
	float BestScore = 0.f;
	FAuthorizationEpisode Episode = LlmProposals::RankAndBuildEpisode(*Model, ExpansionNodeId, Candidates, BestIndex, BestScore);
	const FString EpisodeId = Episode.EpisodeId;
	const FString BestTitle = Candidates[BestIndex].Title;
	Model->AddEpisode(MoveTemp(Episode));
	FrontierQueuedEpisodes++;
	SetLiveStatus(FString::Printf(TEXT("Episode %s queued: '%s' under %s (best of %d candidates, score %.2f)."),
		*EpisodeId, *BestTitle, *ExpansionNodeId, Candidates.Num(), BestScore));
	ContinueFrontier();
}
