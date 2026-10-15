#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NWNiagaraTools.generated.h"

/**
 * Editor tools to build Niagara systems from the game's effects (docs/tech/particles.md).
 * Called by the editor's Python (scripts in tools/ue/); they only do something in an editor build.
 */
UCLASS()
class NWPORT_API UNWNiagaraTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Probe: creates PackagePath (e.g. /Game/NW/FX/NS_Probe) with the engine's template emitter TemplatePath, changes the
	// spawn rate to SpawnRate, compiles and saves. Returns a report (emitters, modules, constant parameters).
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static FString ProbeNiagara(const FString& PackagePath, const FString& TemplatePath, float SpawnRate);

	/**
	 * Builds PackagePath from a JSON recipe (tools/ue/import_fx.py, docs/tech/particles.md). Per emitter:
	 * name, template (engine emitter), disable (modules), add [{usage, script}], params [{usage, name, type, value}],
	 * renderer {kind sprite|mesh|decal, material, mesh, subuv [x, y], mesh_scale}, constants {"Module.Input": value},
	 * curves [{function, index, keys [[t, v]...] or channels {r,g,b,a: keys}}]. Returns the report.
	 */
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static FString BuildEffect(const FString& SpecJson, const FString& PackagePath);
};
