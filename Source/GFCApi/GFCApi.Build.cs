using UnrealBuildTool;

public class GFCApi : ModuleRules
{
	public GFCApi(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"DeveloperSettings",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"JsonUtilities",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// DPAPI (CryptProtectData) protects the stored session token on Windows.
			PublicSystemLibraries.Add("Crypt32.lib");
		}
	}
}
