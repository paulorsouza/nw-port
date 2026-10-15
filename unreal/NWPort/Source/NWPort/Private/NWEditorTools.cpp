#include "NWEditorTools.h"

#include "Animation/AnimMontage.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"

bool UNWEditorTools::UpdateSkeletonRefPoseFromMesh(USkeleton* Skeleton, USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Skeleton || !Mesh)
	{
		return false;
	}
	Skeleton->Modify();
	Skeleton->UpdateReferencePoseFromMesh(Mesh);
	Skeleton->MarkPackageDirty();
	return true;
#else
	return false;
#endif
}

int32 UNWEditorTools::RebuildBlendSpace(UBlendSpace* BlendSpace)
{
#if WITH_EDITOR
	if (!BlendSpace)
	{
		return 0;
	}
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	int32 Valid = 0;
	BlendSpace->ForEachImmutableSample([&Valid](const FBlendSample& S) { Valid += S.bIsValid ? 1 : 0; });
	const FBlendSpaceData& Data = BlendSpace->GetBlendSpaceData();
	UE_LOG(LogTemp, Display, TEXT("NW BlendSpace %s: %d valid samples, %d triangles, %d segments, grid %d"),
		*BlendSpace->GetName(), Valid, Data.Triangles.Num(), Data.Segments.Num(), BlendSpace->GetGridSamples().Num());
	return Data.Triangles.Num() + Data.Segments.Num();
#else
	return 0;
#endif
}

int32 UNWEditorTools::MergeFolderToInstances(UObject* WorldContext, const FString& Folder)
{
#if WITH_EDITOR
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return 0;
	}
	// Group = mesh + instance materials (equal meshes with swapped materials stay separate).
	struct FGroup
	{
		UStaticMesh* Mesh = nullptr;
		TArray<UMaterialInterface*> Materials;
		TArray<FTransform> Transforms;
		bool bCollides = false;
	};
	TMap<FString, FGroup> Groups;
	TArray<AStaticMeshActor*> Merged;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* A = *It;
		const FString Path = A->GetFolderPath().ToString();
		if (Path != Folder && !Path.StartsWith(Folder + TEXT("/")))
		{
			continue;
		}
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		UStaticMesh* Mesh = C ? C->GetStaticMesh() : nullptr;
		if (!Mesh)
		{
			continue;
		}
		FString Key = Mesh->GetPathName();
		TArray<UMaterialInterface*> Materials;
		for (int32 i = 0; i < C->GetNumMaterials(); ++i)
		{
			Materials.Add(C->GetMaterial(i));
			Key += TEXT("|") + (Materials.Last() ? Materials.Last()->GetPathName() : FString());
		}
		FGroup& G = Groups.FindOrAdd(Key);
		G.Mesh = Mesh;
		G.Materials = Materials;
		G.bCollides |= C->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
		G.Transforms.Add(C->GetComponentTransform());
		Merged.Add(A);
	}
	if (Merged.Num() == 0)
	{
		return 0;
	}
	FActorSpawnParameters P;
	P.Name = MakeUniqueObjectName(World->PersistentLevel, AActor::StaticClass(), FName(*FString::Printf(TEXT("Instancias_%s"), *FPaths::GetCleanFilename(Folder))));
	AActor* Holder = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, P);
	USceneComponent* Root = NewObject<USceneComponent>(Holder, TEXT("Root"));
	Holder->SetRootComponent(Root);
	Holder->AddInstanceComponent(Root);
	Root->RegisterComponent();
	Holder->SetActorLabel(P.Name.ToString());
	Holder->SetFolderPath(FName(*Folder));
	int32 Index = 0;
	for (auto& Pair : Groups)
	{
		FGroup& G = Pair.Value;
		UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(Holder, *FString::Printf(TEXT("ISM_%d_%s"), Index++, *G.Mesh->GetName()));
		C->SetMobility(EComponentMobility::Static);
		C->SetStaticMesh(G.Mesh);
		for (int32 i = 0; i < G.Materials.Num(); ++i)
		{
			C->SetMaterial(i, G.Materials[i]);
		}
		C->SetCollisionProfileName(G.bCollides ? TEXT("BlockAll") : TEXT("NoCollision"));
		C->SetupAttachment(Root);
		Holder->AddInstanceComponent(C);
		C->RegisterComponent();
		C->AddInstances(G.Transforms, false, true);
	}
	for (AStaticMeshActor* A : Merged)
	{
		World->EditorDestroyActor(A, true);
	}
	World->MarkPackageDirty();
	UE_LOG(LogTemp, Warning, TEXT("NW merge: %s: %d actors -> %d instanced components"), *Folder, Merged.Num(), Groups.Num());
	return Merged.Num();
#else
	return 0;
#endif
}

bool UNWEditorTools::SetMontageSlot(UAnimMontage* Montage, FName Slot)
{
	if (!Montage || Montage->SlotAnimTracks.Num() == 0)
	{
		return false;
	}
	Montage->Modify();
	Montage->SlotAnimTracks[0].SlotName = Slot;
	Montage->MarkPackageDirty();
	return true;
}

bool UNWEditorTools::MergeMeshBonesToSkeleton(USkeleton* Skeleton, USkeletalMesh* Mesh)
{
#if WITH_EDITOR
	if (!Skeleton || !Mesh)
	{
		return false;
	}
	const int32 Before = Skeleton->GetReferenceSkeleton().GetRawBoneNum();
	Skeleton->Modify();
	if (!Skeleton->MergeAllBonesToBoneTree(Mesh))
	{
		return false;
	}
	const bool bChanged = Skeleton->GetReferenceSkeleton().GetRawBoneNum() != Before;
	if (bChanged)
	{
		Skeleton->MarkPackageDirty();
	}
	return bChanged;
#else
	return false;
#endif
}
