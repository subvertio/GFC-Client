using UnrealBuildTool;

public class GFCClient : ModuleRules
{
	public GFCClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GFCApi" });
	}
}
