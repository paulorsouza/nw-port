using UnrealBuildTool;

public class NWPort : ModuleRules
{
	public NWPort(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Cada .cpp tem helpers proprios em namespace anonimo (Img, Tan, Roman...); no unity build do MSVC eles colidem.
		bUseUnity = false;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AnimGraphRuntime", "AnimationCore", "NetCore", "Json", "RenderCore", "SlateCore", "Slate", "HTTP" });
		// Captura de tela offline (NWUIShotCommandlet): renderizador da Slate para as fontes de runtime.
		PrivateDependencyModuleNames.Add("SlateRHIRenderer");
		// Dados fora de Content (catálogo, telas, textos, fontes): entram no pacote (servidor e cliente).
		RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.UFS);
	}
}
