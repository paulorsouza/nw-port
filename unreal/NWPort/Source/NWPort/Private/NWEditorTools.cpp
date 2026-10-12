#include "NWEditorTools.h"

#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Engine/SkeletalMesh.h"

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
	UE_LOG(LogTemp, Display, TEXT("NW BlendSpace %s: %d amostras válidas, %d triângulos, %d segmentos, grade %d"),
		*BlendSpace->GetName(), Valid, Data.Triangles.Num(), Data.Segments.Num(), BlendSpace->GetGridSamples().Num());
	return Data.Triangles.Num() + Data.Segments.Num();
#else
	return 0;
#endif
}
