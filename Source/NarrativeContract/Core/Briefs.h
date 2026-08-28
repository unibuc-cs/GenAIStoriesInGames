// Story briefs: switchable worlds for the same contract machinery.
// Three built-in briefs (mystery / disaster / science fiction, echoing the
// study's scenario families) plus any user-authored JSON brief dropped into
// <Project>/Briefs/ -- the file format is identical to Save files
// (ContractSerialization format_version 1), so "Save as brief" turns the
// current session into a reusable brief.

#pragma once

#include "CoreMinimal.h"

class FContractModel;

struct FBriefDescriptor
{
	FString Id;
	FString Name;
	FString Genre;
	FString Description;
	TFunction<void(FContractModel&)> Build;
};

namespace Briefs
{
	// Built-in briefs, in display order (index 0 = hydro-station default).
	const TArray<FBriefDescriptor>& BuiltIn();

	// <Project>/Briefs/ (created on demand).
	FString BriefsDir();

	// Filenames (not paths) of *.json briefs in BriefsDir().
	TArray<FString> DiscoverJsonBriefs();

	// Loads a discovered JSON brief into the model. Returns a status line.
	FString LoadJsonBrief(FContractModel& Model, const FString& FileName);

	// Writes the current state into BriefsDir() under a slug of the story
	// title. Returns a status line naming the file.
	FString SaveCurrentAsBrief(const FContractModel& Model);
}
