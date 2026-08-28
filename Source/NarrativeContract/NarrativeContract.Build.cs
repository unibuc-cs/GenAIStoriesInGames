using UnrealBuildTool;

public class NarrativeContract : ModuleRules
{
	public NarrativeContract(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
			"Json",
			"JsonUtilities",
			"HTTP"
		});
	}
}
