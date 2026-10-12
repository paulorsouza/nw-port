using UnrealBuildTool;

public class NWPortServerTarget : TargetRules
{
	public NWPortServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("NWPort");
	}
}
