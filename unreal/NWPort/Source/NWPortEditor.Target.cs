using UnrealBuildTool;

public class NWPortEditorTarget : TargetRules
{
	public NWPortEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NWPort");
	}
}
