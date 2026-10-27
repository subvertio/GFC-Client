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
			"ImageWrapper",
		});

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			// App Links, deep-link intents and the app cache folder (GFCAndroid.cpp).
			PrivateDependencyModuleNames.Add("Launch");
			AdditionalPropertiesForReceipt.Add("AndroidPlugin", System.IO.Path.Combine(ModuleDirectory, "GFCApi_UPL.xml"));
		}

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// DPAPI (CryptProtectData) protects the stored session token on Windows.
			PublicSystemLibraries.Add("Crypt32.lib");
			// Windows "Open" dialog for choosing media to post (FGFCMediaCapture).
			PublicSystemLibraries.Add("Comdlg32.lib");
		}
	}
}
