using UnrealBuildTool;

public class NWPort : ModuleRules
{
	public NWPort(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AnimGraphRuntime", "AnimationCore", "NetCore", "Json", "RenderCore", "SlateCore", "Slate", "HTTP" });
		// Dados fora de Content (catálogo, telas, textos, fontes): entram no pacote (servidor e cliente).
		RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.UFS);
	}
}
