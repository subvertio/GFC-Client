using UnrealBuildTool;

public class GFCClientEditorTarget : TargetRules
{
	public GFCClientEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "GFCClient", "GFCApi" });
	}
}
