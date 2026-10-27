using UnrealBuildTool;

public class GFCClientTarget : TargetRules
{
	public GFCClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "GFCClient", "GFCApi" });
	}
}
