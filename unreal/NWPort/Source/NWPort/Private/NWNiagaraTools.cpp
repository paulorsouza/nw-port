#include "NWNiagaraTools.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraDataInterfaceCurve.h"
#include "NiagaraSpriteRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

FString UNWNiagaraTools::ProbeNiagara(const FString& PackagePath, const FString& TemplatePath, float SpawnRate)
{
#if WITH_EDITOR
	FString Log;
	UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
	if (!Template)
	{
		return TEXT("template not found: ") + TemplatePath;
	}
	UPackage* Package = CreatePackage(*PackagePath);
	const FName Name(*FPackageName::GetShortName(PackagePath));
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);
	FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
	Log += FString::Printf(TEXT("emitters: %d\n"), System->GetEmitterHandles().Num());
	for (const FNiagaraEmitterHandle& H : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = H.GetEmitterData();
		Log += FString::Printf(TEXT("emitter %s\n"), *H.GetUniqueInstanceName());
		if (!Data)
		{
			continue;
		}
		if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource))
		{
			for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
			{
				if (const UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Node))
				{
					Log += FString::Printf(TEXT("  module %s (%s)\n"), *Call->GetFunctionName(),
						Call->FunctionScript ? *Call->FunctionScript->GetPathName() : TEXT("-"));
				}
			}
		}
		// Curves: the DataInterface of the input nodes (class not exported; property via reflection).
		if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Data->GraphSource))
		{
			for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
			{
				const FObjectProperty* Prop = FindFProperty<FObjectProperty>(Node->GetClass(), TEXT("DataInterface"));
				UObject* DI = Prop ? Prop->GetObjectPropertyValue_InContainer(Node) : nullptr;
				if (UNiagaraDataInterfaceCurve* Curve = Cast<UNiagaraDataInterfaceCurve>(DI))
				{
					Log += FString::Printf(TEXT("  curve %s: %d keys -> 0:1, 1:0\n"), *Node->GetName(), Curve->Curve.GetNumKeys());
					Curve->Modify();
					Curve->Curve.Reset();
					Curve->Curve.AddKey(0.f, 1.f);
					Curve->Curve.AddKey(1.f, 0.f);
					Curve->UpdateLUT();
				}
			}
		}
		for (UNiagaraRendererProperties* R : Data->GetRenderers())
		{
			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(R))
			{
				Sprite->Modify();
				Sprite->SubImageSize = FVector2D(2.0, 2.0);
				Sprite->Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/NW/Materials/M_NW_FX.M_NW_FX"));
				Log += FString::Printf(TEXT("  sprite: atlas 2x2, material %s\n"), Sprite->Material ? *Sprite->Material->GetName() : TEXT("-"));
			}
		}
		TArray<UNiagaraScript*> Scripts;
		Data->GetScripts(Scripts, false);
		for (UNiagaraScript* Script : Scripts)
		{
			TArray<FNiagaraVariable> Params;
			Script->RapidIterationParameters.GetParameters(Params);
			for (const FNiagaraVariable& P : Params)
			{
				Log += FString::Printf(TEXT("  param %s [%s]\n"), *P.GetName().ToString(), *P.GetType().GetName());
				if (P.GetName().ToString().EndsWith(TEXT("SpawnRate.SpawnRate")) && P.GetType() == FNiagaraTypeDefinition::GetFloatDef())
				{
					Script->RapidIterationParameters.SetParameterValue(SpawnRate, P);
					Log += FString::Printf(TEXT("    -> %.1f\n"), SpawnRate);
				}
			}
		}
	}
	System->RequestCompile(false);
	System->WaitForCompilationComplete(true, false);
	FAssetRegistryModule::AssetCreated(System);
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	const bool bSaved = UPackage::SavePackage(Package, System, *File, Args);
	Log += FString::Printf(TEXT("saved: %s %s\n"), bSaved ? TEXT("yes") : TEXT("NO"), *File);
	return Log;
#else
	return TEXT("editor only");
#endif
}

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "NiagaraDataInterfaceColorCurve.h"
#include "NiagaraDecalRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeOutput.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "Engine/StaticMesh.h"

namespace NWNiagara
{
	static ENiagaraScriptUsage Usage(const FString& S)
	{
		return S == TEXT("EmitterSpawn") ? ENiagaraScriptUsage::EmitterSpawnScript
			: S == TEXT("EmitterUpdate") ? ENiagaraScriptUsage::EmitterUpdateScript
			: S == TEXT("ParticleSpawn") ? ENiagaraScriptUsage::ParticleSpawnScript
			: ENiagaraScriptUsage::ParticleUpdateScript;
	}

	static UNiagaraGraph* Graph(FVersionedNiagaraEmitterData* Data)
	{
		UNiagaraScriptSource* Source = Data ? Cast<UNiagaraScriptSource>(Data->GraphSource) : nullptr;
		return Source ? Source->NodeGraph : nullptr;
	}

	static TArray<double> Numbers(const TSharedPtr<FJsonValue>& V)
	{
		TArray<double> Out;
		if (V->Type == EJson::Array)
		{
			for (const TSharedPtr<FJsonValue>& X : V->AsArray())
			{
				Out.Add(X->AsNumber());
			}
		}
		else if (V->Type == EJson::Boolean)
		{
			Out.Add(V->AsBool() ? 1.0 : 0.0);
		}
		else
		{
			Out.Add(V->AsNumber());
		}
		return Out;
	}

	// Writes the constant to the rapid iteration parameter, with its type.
	static bool SetConstant(FNiagaraParameterStore& Store, const FNiagaraVariable& P, const TArray<double>& V)
	{
		const FNiagaraTypeDefinition& T = P.GetType();
		auto At = [&V](int32 i) { return float(V.IsValidIndex(i) ? V[i] : (V.Num() ? V.Last() : 0.0)); };
		if (T == FNiagaraTypeDefinition::GetFloatDef()) return Store.SetParameterValue(At(0), P);
		if (T == FNiagaraTypeDefinition::GetIntDef()) return Store.SetParameterValue(int32(At(0)), P);
		if (T == FNiagaraTypeDefinition::GetBoolDef()) { FNiagaraBool B; B.SetValue(At(0) != 0.f); return Store.SetParameterValue(B, P); }
		if (T == FNiagaraTypeDefinition::GetVec2Def()) return Store.SetParameterValue(FVector2f(At(0), At(1)), P);
		if (T == FNiagaraTypeDefinition::GetVec3Def() || T == FNiagaraTypeDefinition::GetPositionDef()) return Store.SetParameterValue(FVector3f(At(0), At(1), At(2)), P);
		if (T == FNiagaraTypeDefinition::GetVec4Def()) return Store.SetParameterValue(FVector4f(At(0), At(1), At(2), At(3)), P);
		if (T == FNiagaraTypeDefinition::GetColorDef()) return Store.SetParameterValue(FLinearColor(At(0), At(1), At(2), V.Num() > 3 ? At(3) : 1.f), P);
		return false;
	}

	static void Keys(FRichCurve& C, const TArray<TSharedPtr<FJsonValue>>& Ks)
	{
		C.Reset();
		for (const TSharedPtr<FJsonValue>& K : Ks)
		{
			const TArray<TSharedPtr<FJsonValue>>& TV = K->AsArray();
			if (TV.Num() >= 2)
			{
				const FKeyHandle H = C.AddKey(float(TV[0]->AsNumber()), float(TV[1]->AsNumber()));
				C.SetKeyInterpMode(H, RCIM_Linear);
			}
		}
	}

	// Input nodes with a data interface linked to a function call with this name, in graph order.
	static TArray<UNiagaraDataInterface*> CurvesOf(UNiagaraGraph* G, const FString& Function)
	{
		TArray<UNiagaraDataInterface*> Out;
		for (UEdGraphNode* Node : G->Nodes)
		{
			const FObjectProperty* Prop = FindFProperty<FObjectProperty>(Node->GetClass(), TEXT("DataInterface"));
			UNiagaraDataInterface* DI = Prop ? Cast<UNiagaraDataInterface>(Prop->GetObjectPropertyValue_InContainer(Node)) : nullptr;
			if (!DI)
			{
				continue;
			}
			// The curve links to the module through an override node (ParameterMapSet): follows up to two links.
			bool bMatch = Function == TEXT("*");
			for (UEdGraphPin* Pin : Node->Pins)
			{
				for (UEdGraphPin* L : Pin->LinkedTo)
				{
					UEdGraphNode* Next = L ? L->GetOwningNode() : nullptr;
					const UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Next);
					bMatch |= Call && Call->GetFunctionName() == Function;
					for (UEdGraphPin* P2 : Next ? Next->Pins : TArray<UEdGraphPin*>())
					{
						for (UEdGraphPin* L2 : P2->LinkedTo)
						{
							const UNiagaraNodeFunctionCall* C2 = L2 ? Cast<UNiagaraNodeFunctionCall>(L2->GetOwningNode()) : nullptr;
							bMatch |= C2 && C2->GetFunctionName() == Function;
						}
					}
				}
			}
			if (bMatch)
			{
				Out.AddUnique(DI);
			}
		}
		return Out;
	}
}
#endif

FString UNWNiagaraTools::BuildEffect(const FString& SpecJson, const FString& PackagePath)
{
#if WITH_EDITOR
	using namespace NWNiagara;
	FString Log;
	TSharedPtr<FJsonObject> Spec;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(SpecJson), Spec) || !Spec.IsValid())
	{
		return TEXT("receita inválida");
	}
	// Redo: loads the whole package (saving a partial package crashes the editor) and moves the old system out of the way.
	UPackage* Package = FPackageName::DoesPackageExist(PackagePath) ? LoadPackage(nullptr, *PackagePath, LOAD_None) : nullptr;
	if (Package)
	{
		Package->FullyLoad();
	}
	else
	{
		Package = CreatePackage(*PackagePath);
	}
	const FName Name(*FPackageName::GetShortName(PackagePath));
	if (UObject* Old = FindObject<UObject>(Package, *Name.ToString()))
	{
		Old->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		Old->ClearFlags(RF_Public | RF_Standalone);
	}
	UNiagaraSystem* System = NewObject<UNiagaraSystem>(Package, Name, RF_Public | RF_Standalone | RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);

	struct FBuilt { FGuid Id; TSharedPtr<FJsonObject> Spec; FString Name; };
	TArray<FBuilt> Built;
	for (const TSharedPtr<FJsonValue>& EV : Spec->GetArrayField(TEXT("emitters")))
	{
		const TSharedPtr<FJsonObject> E = EV->AsObject();
		UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, *E->GetStringField(TEXT("template")));
		if (!Template)
		{
			Log += TEXT("modelo não encontrado: ") + E->GetStringField(TEXT("template")) + TEXT("\n");
			continue;
		}
		const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
		FNiagaraEmitterHandle* H = System->GetEmitterHandles().FindByPredicate([&Id](const FNiagaraEmitterHandle& X) { return X.GetId() == Id; });
		if (!H)
		{
			continue;
		}
		H->SetName(FName(*E->GetStringField(TEXT("name"))), *System);
		FVersionedNiagaraEmitterData* Data = H->GetEmitterData();
		UNiagaraGraph* G = Graph(Data);
		if (!Data || !G)
		{
			continue;
		}
		const FString EName = H->GetUniqueInstanceName();
		Built.Add({Id, E, EName});
		Log += TEXT("emissor ") + EName + TEXT("\n");
		// Modules to disable.
		const TArray<TSharedPtr<FJsonValue>>* Disable;
		if (E->TryGetArrayField(TEXT("disable"), Disable))
		{
			for (UEdGraphNode* Node : G->Nodes)
			{
				if (UNiagaraNodeFunctionCall* Call = Cast<UNiagaraNodeFunctionCall>(Node))
				{
					for (const TSharedPtr<FJsonValue>& D : *Disable)
					{
						if (Call->GetFunctionName() == D->AsString())
						{
							FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Call, false);
							Log += TEXT("  desligado ") + D->AsString() + TEXT("\n");
						}
					}
				}
			}
		}
		// Modules to add.
		const TArray<TSharedPtr<FJsonValue>>* Add;
		if (E->TryGetArrayField(TEXT("add"), Add))
		{
			for (const TSharedPtr<FJsonValue>& AV : *Add)
			{
				const TSharedPtr<FJsonObject> A = AV->AsObject();
				UNiagaraScript* Module = LoadObject<UNiagaraScript>(nullptr, *A->GetStringField(TEXT("script")));
				UNiagaraNodeOutput* Out = G->FindEquivalentOutputNode(Usage(A->GetStringField(TEXT("usage"))));
				if (Module && Out)
				{
					UNiagaraNodeFunctionCall* Call = FNiagaraStackGraphUtilities::AddScriptModuleToStack(Module, *Out);
					Log += TEXT("  módulo ") + (Call ? Call->GetFunctionName() : FString(TEXT("FALHOU"))) + TEXT("\n");
				}
				else
				{
					Log += TEXT("  módulo não encontrado: ") + A->GetStringField(TEXT("script")) + TEXT("\n");
				}
			}
		}
		// Particle parameters directly (Particles.DecalSize...).
		const TArray<TSharedPtr<FJsonValue>>* Params;
		if (E->TryGetArrayField(TEXT("params"), Params))
		{
			for (const TSharedPtr<FJsonValue>& PV : *Params)
			{
				const TSharedPtr<FJsonObject> P = PV->AsObject();
				const FString Type = P->GetStringField(TEXT("type"));
				const FNiagaraTypeDefinition Def = Type == TEXT("vec3") ? FNiagaraTypeDefinition::GetVec3Def()
					: Type == TEXT("quat") ? FNiagaraTypeDefinition::GetQuatDef()
					: Type == TEXT("color") ? FNiagaraTypeDefinition::GetColorDef() : FNiagaraTypeDefinition::GetFloatDef();
				UNiagaraNodeOutput* Out = G->FindEquivalentOutputNode(Usage(P->GetStringField(TEXT("usage"))));
				if (Out)
				{
					FNiagaraStackGraphUtilities::AddParameterModuleToStack({FNiagaraVariable(Def, FName(*P->GetStringField(TEXT("name"))))}, *Out,
						INDEX_NONE, {P->GetStringField(TEXT("value"))});
					Log += TEXT("  parâmetro ") + P->GetStringField(TEXT("name")) + TEXT(" = ") + P->GetStringField(TEXT("value")) + TEXT("\n");
				}
			}
		}
		// Renderer.
		const TSharedPtr<FJsonObject>* R;
		if (E->TryGetObjectField(TEXT("renderer"), R))
		{
			const FString Kind = (*R)->GetStringField(TEXT("kind"));
			UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *(*R)->GetStringField(TEXT("material")));
			const FGuid Ver = H->GetInstance().Version;
			UNiagaraEmitter* Emitter = H->GetInstance().Emitter;
			if (Kind == TEXT("sprite"))
			{
				for (UNiagaraRendererProperties* RP : Data->GetRenderers())
				{
					if (UNiagaraSpriteRendererProperties* S = Cast<UNiagaraSpriteRendererProperties>(RP))
					{
						S->Material = Mat;
						const TArray<TSharedPtr<FJsonValue>>* Sub;
						if ((*R)->TryGetArrayField(TEXT("subuv"), Sub) && Sub->Num() == 2)
						{
							S->SubImageSize = FVector2D((*Sub)[0]->AsNumber(), (*Sub)[1]->AsNumber());
						}
						FString Align;
						if ((*R)->TryGetStringField(TEXT("alignment"), Align) && Align == TEXT("velocity"))
						{
							// The game's OrientToVelocity/Facing=Shape: the sprite lies along the direction of motion (streak).
							S->Alignment = ENiagaraSpriteAlignment::VelocityAligned;
						}
					}
				}
			}
			else
			{
				for (UNiagaraRendererProperties* RP : TArray<UNiagaraRendererProperties*>(Data->GetRenderers()))
				{
					Emitter->RemoveRenderer(RP, Ver);
				}
				if (Kind == TEXT("mesh"))
				{
					UNiagaraMeshRendererProperties* M = NewObject<UNiagaraMeshRendererProperties>(Emitter, NAME_None, RF_Transactional);
					FNiagaraMeshRendererMeshProperties MP;
					MP.Mesh = LoadObject<UStaticMesh>(nullptr, *(*R)->GetStringField(TEXT("mesh")));
					double Scale = 1.0;
					(*R)->TryGetNumberField(TEXT("mesh_scale"), Scale);
					MP.Scale = FVector(Scale);
					M->Meshes = {MP};
					M->bOverrideMaterials = true;
					FNiagaraMeshMaterialOverride O;
					O.ExplicitMat = Mat;
					M->OverrideMaterials = {O};
					Emitter->AddRenderer(M, Ver);
				}
				else
				{
					UNiagaraDecalRendererProperties* D = NewObject<UNiagaraDecalRendererProperties>(Emitter, NAME_None, RF_Transactional);
					D->Material = Mat;
					Emitter->AddRenderer(D, Ver);
				}
			}
			Log += TEXT("  renderizador ") + Kind + TEXT(" ") + (Mat ? Mat->GetName() : FString(TEXT("(sem material)"))) + TEXT("\n");
		}
	}
	// Compiles once so the new modules create their inputs; then the constants and the curves.
	System->RequestCompile(false);
	System->WaitForCompilationComplete(true, false);
	for (const FBuilt& B : Built)
	{
		FNiagaraEmitterHandle* H = System->GetEmitterHandles().FindByPredicate([&B](const FNiagaraEmitterHandle& X) { return X.GetId() == B.Id; });
		FVersionedNiagaraEmitterData* Data = H ? H->GetEmitterData() : nullptr;
		if (!Data)
		{
			continue;
		}
		const TSharedPtr<FJsonObject>* Consts;
		if (B.Spec->TryGetObjectField(TEXT("constants"), Consts))
		{
			TArray<UNiagaraScript*> Scripts;
			Data->GetScripts(Scripts, false);
			// Emitter modules (EmitterState, SpawnRate, SpawnBurst) run compiled in the system scripts: writes there too.
			Scripts.Add(System->GetSystemSpawnScript());
			Scripts.Add(System->GetSystemUpdateScript());
			for (const auto& KV : (*Consts)->Values)
			{
				const FString Suffix = FString(TEXT(".")) + B.Name + TEXT(".") + FString(*KV.Key);
				bool bSet = false;
				for (UNiagaraScript* Script : Scripts)
				{
					if (!Script)
					{
						continue;
					}
					TArray<FNiagaraVariable> Ps;
					Script->RapidIterationParameters.GetParameters(Ps);
					for (const FNiagaraVariable& P : Ps)
					{
						if (P.GetName().ToString().EndsWith(Suffix))
						{
							bSet |= SetConstant(Script->RapidIterationParameters, P, Numbers(KV.Value));
						}
					}
				}
				Log += FString::Printf(TEXT("  %s %s\n"), bSet ? TEXT("=") : TEXT("? (não achado)"), *FString(*KV.Key));
				if (!bSet)
				{
					// Lists the module's inputs, to fix the name in the recipe.
					FString Module;
					FString(*KV.Key).Split(TEXT("."), &Module, nullptr);
					for (UNiagaraScript* Script : Scripts)
					{
						if (!Script)
						{
							continue;
						}
						TArray<FNiagaraVariable> Ps;
						Script->RapidIterationParameters.GetParameters(Ps);
						for (const FNiagaraVariable& P : Ps)
						{
							if (P.GetName().ToString().Contains(FString(TEXT(".")) + Module + TEXT(".")))
							{
								Log += TEXT("      tem ") + P.GetName().ToString() + TEXT("\n");
							}
						}
					}
				}
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Curves;
		if (B.Spec->TryGetArrayField(TEXT("curves"), Curves))
		{
			UNiagaraGraph* G = Graph(Data);
			for (const TSharedPtr<FJsonValue>& CV : *Curves)
			{
				const TSharedPtr<FJsonObject> C = CV->AsObject();
				const TArray<UNiagaraDataInterface*> DIs = CurvesOf(G, C->GetStringField(TEXT("function")));
				const int32 Index = int32(C->GetNumberField(TEXT("index")));
				UNiagaraDataInterface* DI = DIs.IsValidIndex(Index) ? DIs[Index] : nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Ks;
				const TSharedPtr<FJsonObject>* Ch;
				if (UNiagaraDataInterfaceCurve* F = Cast<UNiagaraDataInterfaceCurve>(DI); F && C->TryGetArrayField(TEXT("keys"), Ks))
				{
					F->Modify();
					Keys(F->Curve, *Ks);
					F->UpdateLUT();
					Log += TEXT("  curva ") + C->GetStringField(TEXT("function")) + TEXT("\n");
				}
				else if (UNiagaraDataInterfaceColorCurve* K = Cast<UNiagaraDataInterfaceColorCurve>(DI); K && C->TryGetObjectField(TEXT("channels"), Ch))
				{
					K->Modify();
					FRichCurve* Chans[] = {&K->RedCurve, &K->GreenCurve, &K->BlueCurve, &K->AlphaCurve};
					const TCHAR* Names[] = {TEXT("r"), TEXT("g"), TEXT("b"), TEXT("a")};
					for (int32 i = 0; i < 4; ++i)
					{
						if ((*Ch)->TryGetArrayField(Names[i], Ks))
						{
							Keys(*Chans[i], *Ks);
						}
					}
					K->UpdateLUT();
					Log += TEXT("  curva de cor ") + C->GetStringField(TEXT("function")) + TEXT("\n");
				}
				else
				{
					Log += FString::Printf(TEXT("  curva não achada: %s[%d] (%d no grafo)\n"), *C->GetStringField(TEXT("function")), Index, DIs.Num());
				}
			}
		}
	}
	// System state (SystemState): the recipe's "system_constants" constants, with the list of what exists in the report.
	const TSharedPtr<FJsonObject>* SysConsts = nullptr;
	Spec->TryGetObjectField(TEXT("system_constants"), SysConsts);
	for (UNiagaraScript* Script : {System->GetSystemSpawnScript(), System->GetSystemUpdateScript()})
	{
		if (!Script)
		{
			continue;
		}
		TArray<FNiagaraVariable> Ps;
		Script->RapidIterationParameters.GetParameters(Ps);
		for (const FNiagaraVariable& P : Ps)
		{
			Log += TEXT("  sistema tem ") + P.GetName().ToString() + TEXT("\n");
			if (SysConsts)
			{
				for (const auto& KV : (*SysConsts)->Values)
				{
					if (P.GetName().ToString().EndsWith(FString(TEXT(".")) + FString(*KV.Key)))
					{
						SetConstant(Script->RapidIterationParameters, P, Numbers(KV.Value));
						Log += TEXT("  sistema = ") + FString(*KV.Key) + TEXT("\n");
					}
				}
			}
		}
	}
	System->RequestCompile(false);
	System->WaitForCompilationComplete(true, false);
	FAssetRegistryModule::AssetCreated(System);
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	const FString File = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
	Log += FString::Printf(TEXT("salvo: %s\n"), UPackage::SavePackage(Package, System, *File, Args) ? TEXT("sim") : TEXT("NÃO"));
	return Log;
#else
	return TEXT("só no editor");
#endif
}
