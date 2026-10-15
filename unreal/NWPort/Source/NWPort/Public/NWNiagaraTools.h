#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NWNiagaraTools.generated.h"

/**
 * Ferramentas de editor para montar sistemas Niagara a partir dos efeitos do jogo (docs/tech/particles.md).
 * Chamadas pelo Python do editor (scripts em tools/ue/); só fazem algo em build de editor.
 */
UCLASS()
class NWPORT_API UNWNiagaraTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Prova: cria PackagePath (ex. /Game/NW/FX/NS_Probe) com o emissor modelo do motor TemplatePath, muda a taxa de
	// spawn para SpawnRate, compila e salva. Devolve um relatório (emissores, módulos, parâmetros constantes).
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static FString ProbeNiagara(const FString& PackagePath, const FString& TemplatePath, float SpawnRate);

	/**
	 * Monta PackagePath a partir de uma receita JSON (tools/ue/import_fx.py, docs/tech/particles.md). Por emissor:
	 * name, template (emissor do motor), disable (módulos), add [{usage, script}], params [{usage, name, type, value}],
	 * renderer {kind sprite|mesh|decal, material, mesh, subuv [x, y], mesh_scale}, constants {"Modulo.Entrada": valor},
	 * curves [{function, index, keys [[t, v]...] ou channels {r,g,b,a: keys}}]. Devolve o relatório.
	 */
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static FString BuildEffect(const FString& SpecJson, const FString& PackagePath);
};
