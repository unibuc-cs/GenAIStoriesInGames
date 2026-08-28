using UnrealBuildTool;
using System.Collections.Generic;

public class NarrativeContractTarget : TargetRules
{
	public NarrativeContractTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NarrativeContract");
	}
}
