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
		// FWidgetRenderer (captura offline das telas em Slate).
		PrivateDependencyModuleNames.Add("UMG");
		// Dados fora de Content (catálogo, telas, textos, fontes): entram no pacote (servidor e cliente).
		RuntimeDependencies.Add("$(ProjectDir)/Data/...", StagedFileType.UFS);
		// Efeitos do jogo em Niagara (docs/particulas-estudo.md); a montagem dos sistemas é só no editor.
		PublicDependencyModuleNames.Add("Niagara");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "NiagaraEditor" });
		}
	}
}
