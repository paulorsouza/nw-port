#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NWEditorTools.generated.h"

class USkeleton;
class USkeletalMesh;
class UBlendSpace;

// Funções do editor que o Unreal não expõe ao Python e os scripts de import precisam.
UCLASS()
class NWPORT_API UNWEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Copia a pose de referência da malha para o esqueleto (reimportar a malha não atualiza o esqueleto).
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static bool UpdateSkeletonRefPoseFromMesh(USkeleton* Skeleton, USkeletalMesh* Mesh);

	// Monta a grade de interpolação depois de mudar as amostras por script (o editor só faz isso na interface).
	// Devolve triângulos + segmentos da triangulação; 0 = falhou (detalhe no log, LogTemp "NW BlendSpace").
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static int32 RebuildBlendSpace(UBlendSpace* BlendSpace);
};
