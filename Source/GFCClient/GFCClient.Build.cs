using UnrealBuildTool;

public class GFCClient : ModuleRules
{
	public GFCClient(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Headers live beside sources (UI/...), not in Public/Private.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
			"SlateCore",
			"GFCApi",
			"ProceduralMeshComponent",
			"MediaAssets",
			"AudioMixer",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ApplicationCore", // clipboard
			"HTTP",
			"ImageWrapper", // 3D preview thumbnails
			"Json",
		});
	}
}
