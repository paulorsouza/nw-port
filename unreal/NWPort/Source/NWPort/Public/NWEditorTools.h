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

	// Junta os StaticMeshActor de uma pasta do Outliner (e subpastas) num ator só, com um componente instanciado
	// por malha + materiais, e apaga os originais. O cenário da região tinha 190 mil atores e o editor esgotava a
	// memória ao abrir o mapa. Devolve quantos atores foram juntados.
	UFUNCTION(BlueprintCallable, Category = "NW|Editor", meta = (WorldContext = "WorldContext"))
	static int32 MergeFolderToInstances(UObject* WorldContext, const FString& Folder);

	// Troca o slot da montagem (ex.: "UpperBody" para os fragmentos de corpo superior do arco). O Python não
	// enxerga SlotAnimTracks.
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static bool SetMontageSlot(class UAnimMontage* Montage, FName Slot);
};
