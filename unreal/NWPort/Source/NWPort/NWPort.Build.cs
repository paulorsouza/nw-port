using UnrealBuildTool;

public class NWPort : ModuleRules
{
	public NWPort(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Each .cpp has its own helpers in an anonymous namespace (Img, Tan, Roman...); in the MSVC unity build they collide.
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AnimGraphRuntime", "AnimationCore", "NetCore", "Json", "RenderCore", "SlateCore", "Slate", "HTTP" });
		// Offline screenshot (NWUIShotCommandlet): Slate renderer for the runtime fonts.
		PrivateDependencyModuleNames.Add("SlateRHIRenderer");
		// FWidgetRenderer (offline capture of the Slate screens).
		PrivateDependencyModuleNames.Add("UMG");
		// Data outside Content (catalog, screens, texts, fonts): goes into the package (server and client).
		RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.UFS);
		// Game effects in Niagara (docs/tech/particles.md); building the systems happens only in the editor.
		PublicDependencyModuleNames.Add("Niagara");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "NiagaraEditor" });
		}
	}
}
