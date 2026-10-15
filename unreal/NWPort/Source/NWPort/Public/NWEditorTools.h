#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NWEditorTools.generated.h"

class USkeleton;
class USkeletalMesh;
class UBlendSpace;

// Editor functions that Unreal does not expose to Python and that the import scripts need.
UCLASS()
class NWPORT_API UNWEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Copies the mesh's reference pose to the skeleton (reimporting the mesh does not update the skeleton).
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static bool UpdateSkeletonRefPoseFromMesh(USkeleton* Skeleton, USkeletalMesh* Mesh);

	// Builds the interpolation grid after changing the samples by script (the editor only does it in the UI).
	// Returns triangles + segments of the triangulation; 0 = failed (details in the log, LogTemp "NW BlendSpace").
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static int32 RebuildBlendSpace(UBlendSpace* BlendSpace);

	// Merges the StaticMeshActors of an Outliner folder (and subfolders) into a single actor, with one instanced component
	// per mesh + materials, and deletes the originals. The region's scenery had 190 thousand actors and the editor ran out
	// of memory opening the map. Returns how many actors were merged.
	UFUNCTION(BlueprintCallable, Category = "NW|Editor", meta = (WorldContext = "WorldContext"))
	static int32 MergeFolderToInstances(UObject* WorldContext, const FString& Folder);

	// Changes the montage slot (e.g. "UpperBody" for the bow's upper-body fragments). Python cannot
	// see SlotAnimTracks.
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static bool SetMontageSlot(class UAnimMontage* Montage, FName Slot);

	// Adds to the skeleton the mesh bones it lacks (NPC with an extra bone, e.g. c_jaw_BND). Without this,
	// Unreal adds them only in memory on load and warns "missing bones" every time. Returns true if it changed.
	UFUNCTION(BlueprintCallable, Category = "NW|Editor")
	static bool MergeMeshBonesToSkeleton(USkeleton* Skeleton, USkeletalMesh* Mesh);
};
