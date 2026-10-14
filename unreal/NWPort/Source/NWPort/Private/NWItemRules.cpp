#include "NWItemRules.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

const FNWItemRules& FNWItemRules::Get()
{
	static FNWItemRules Instance;
	return Instance;
}

FNWItemRules::FNWItemRules()
{
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/Items/regras_itens.json");
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Itens: não li %s"), *File);
		return;
	}
	Root->TryGetNumberField(TEXT("nivel_fabricar"), CraftLevel);
	Root->TryGetNumberField(TEXT("assinatura_qualidade"), SignQuality);
	Root->TryGetNumberField(TEXT("atributo_por_gs"), AttrPerGs);
	Root->TryGetNumberField(TEXT("perks_por_item"), PerksPerItem);
	Root->TryGetNumberField(TEXT("perks_por_item_anel"), PerksPerRing);
	Root->TryGetNumberField(TEXT("perks_habilidade_max"), AbilityPerksMax);
	Root->TryGetNumberField(TEXT("gs_maximo"), GsMax);
	Root->TryGetNumberField(TEXT("qualidade_min_oficio_max"), QualityMinCraftMax);
	Root->TryGetNumberField(TEXT("qualidade_min_tudo_max"), QualityMinAllMax);
	const TSharedPtr<FJsonObject>* Obj = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
	if (Root->TryGetArrayField(TEXT("atributos"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			Attributes.Add(FName(*V->AsString()));
		}
	}
	if (Root->TryGetArrayField(TEXT("arma_cura"), Arr))
	{
		for (const TSharedPtr<FJsonValue>& V : *Arr)
		{
			HealWeapons.Add(FName(*V->AsString()));
		}
	}
	if (Root->TryGetObjectField(TEXT("perks"), Obj))
	{
		for (const auto& P : (*Obj)->Values)
		{
			const TSharedPtr<FJsonObject> O = P.Value->AsObject();
			FPerk& Perk = Perks.Add(FName(*P.Key));
			for (const TPair<const TCHAR*, TMap<FString, FString>*>& F : {TPair<const TCHAR*, TMap<FString, FString>*>(TEXT("nome"), &Perk.Name),
				TPair<const TCHAR*, TMap<FString, FString>*>(TEXT("efeito"), &Perk.Effect)})
			{
				const TSharedPtr<FJsonObject>* L = nullptr;
				if (O->TryGetObjectField(F.Key, L))
				{
					for (const auto& T : (*L)->Values)
					{
						F.Value->Add(FString(T.Key), T.Value->AsString());
					}
				}
			}
			O->TryGetNumberField(TEXT("min"), Perk.Min);
			O->TryGetNumberField(TEXT("max"), Perk.Max);
			O->TryGetStringField(TEXT("icone"), Perk.Icon);
		}
	}
	if (Root->TryGetObjectField(TEXT("gema_itens"), Obj))
	{
		for (const auto& G : (*Obj)->Values)
		{
			const TSharedPtr<FJsonObject> O = G.Value->AsObject();
			FGem& Gem = Gems.Add(FName(*G.Key));
			Gem.Class = O->GetStringField(TEXT("classe"));
			Gem.Raw = FName(*O->GetStringField(TEXT("bruta")));
			O->TryGetBoolField(TEXT("vidro"), Gem.bGlass);
			for (const auto& E : O->GetObjectField(TEXT("efeitos"))->Values)
			{
				Gem.Effects.Add({FName(*E.Key), float(E.Value->AsNumber())});
			}
		}
	}
	if (Root->TryGetObjectField(TEXT("gema_classes"), Obj))
	{
		for (const auto& C : (*Obj)->Values)
		{
			TArray<FString>& L = GemClasses.Add(FString(C.Key));
			for (const TSharedPtr<FJsonValue>& V : C.Value->AsArray())
			{
				L.Add(V->AsString());
			}
		}
	}
	if (Root->TryGetObjectField(TEXT("efeitos_gema"), Obj))
	{
		for (const auto& E : (*Obj)->Values)
		{
			TMap<FString, FString>& T = GemEffectText.Add(FName(*E.Key));
			for (const auto& L : E.Value->AsObject()->Values)
			{
				T.Add(FString(L.Key), L.Value->AsString());
			}
		}
	}
	for (const TPair<const TCHAR*, bool>& Field : {TPair<const TCHAR*, bool>(TEXT("equipamento"), true), TPair<const TCHAR*, bool>(TEXT("habilidade"), false)})
	{
		if (!Root->TryGetObjectField(Field.Key, Obj))
		{
			continue;
		}
		for (const auto& E : (*Obj)->Values)
		{
			TArray<FName> List;
			for (const TSharedPtr<FJsonValue>& V : E.Value->AsArray())
			{
				List.Add(FName(*V->AsString()));
			}
			if (Field.Value)
			{
				Equipment.Add(FString(E.Key), List);
			}
			else
			{
				Ability.Add(FName(*E.Key), List);
			}
		}
	}
}

FString FNWItemRules::EquipClass(const FNWItemDef& D)
{
	if (D.Set == TEXT("Shield"))
	{
		return TEXT("escudo");
	}
	if (D.Set == TEXT("Pick") || D.Set == TEXT("Sickle") || D.Set == TEXT("LoggingAxe") || D.Set == TEXT("SkinningKnife"))
	{
		return TEXT("ferramenta");
	}
	return D.Type == TEXT("weapon") ? TEXT("arma") : FString();
}

bool FNWItemRules::HasSocket(const FNWItemDef& D, const FNWItem& Item)
{
	// 1 slot por arma, escudo e armadura fabricados; joias e ferramentas não têm (docs/itens.md).
	const FString Class = EquipClass(D);
	return Item.Quality >= 0.f && (Class == TEXT("arma") || Class == TEXT("escudo") || Class == TEXT("armadura"));
}

bool FNWItemRules::CanSocket(const FNWItemDef& D, const FNWItem& Item, FName GemId) const
{
	const FGem* G = Gems.Find(GemId);
	const TArray<FString>* Classes = G ? GemClasses.Find(G->Class) : nullptr;
	return G && Classes && HasSocket(D, Item) && Classes->Contains(EquipClass(D));
}

TArray<FString> FNWItemRules::GemLines(FName GemId) const
{
	TArray<FString> Out;
	const FGem* G = Gems.Find(GemId);
	if (!G)
	{
		return Out;
	}
	for (const TPair<FName, float>& E : G->Effects)
	{
		const TMap<FString, FString>* T = GemEffectText.Find(E.Key);
		const FString* L = T ? T->Find(NWText::Lang()) : nullptr;
		FString Line = L ? *L : E.Key.ToString() + TEXT(" {v}");
		// Uma casa decimal só quando precisa (2,5 / 5).
		FString V = FMath::IsNearlyEqual(E.Value, FMath::RoundToFloat(E.Value)) ? FString::Printf(TEXT("%.0f"), E.Value) : FString::Printf(TEXT("%.1f"), E.Value);
		if (NWText::Lang() == TEXT("pt-br"))
		{
			V.ReplaceInline(TEXT("."), TEXT(","));
		}
		Out.Add(Line.Replace(TEXT("{v}"), *V));
	}
	return Out;
}

FName FNWItemRules::UnsocketCost(FName GemId, int32& OutQty) const
{
	const FGem* G = Gems.Find(GemId);
	OutQty = G ? 1 : 0;
	return G ? G->Raw : NAME_None;
}

TArray<FName> FNWItemRules::PerkPool(const FNWItemDef& D) const
{
	TArray<FName> Pool;
	const FString Class = EquipClass(D);
	if (const TArray<FName>* E = Equipment.Find(Class))
	{
		Pool.Append(*E);
	}
	if (const TArray<FName>* A = Ability.Find(D.Set))
	{
		Pool.Append(*A);   // habilidade só na arma dela
	}
	if (HealWeapons.Contains(D.Set))
	{
		Pool.AddUnique(TEXT("CuraCausada"));
	}
	return Pool;
}

float FNWItemRules::PerkValue(FName Perk, float Quality) const
{
	const FPerk* P = Perks.Find(Perk);
	if (!P || P->Max <= 0.f)
	{
		return 0.f;
	}
	const float Q = FMath::Clamp(Quality, 0.f, 100.f) / 100.f;
	return FMath::RoundToFloat((P->Min + (P->Max - P->Min) * Q) * 10.f) / 10.f;   // uma casa decimal
}

FString FNWItemRules::PerkText(FName Perk, float Quality, bool bName) const
{
	const FPerk* P = Perks.Find(Perk);
	if (!P)
	{
		return Perk.ToString();
	}
	const TMap<FString, FString>& M = bName ? P->Name : P->Effect;
	const FString* T = M.Find(NWText::Lang());
	FString Out = T ? *T : (M.Num() ? M.CreateConstIterator()->Value : Perk.ToString());
	if (!bName && P->Max > 0.f)
	{
		Out.ReplaceInline(TEXT("{v}"), *FString::Printf(TEXT("%.1f"), PerkValue(Perk, Quality)));
	}
	return Out;
}

int32 FNWItemRules::AttrValue(int32 GearScore) const
{
	return FMath::RoundToInt(GearScore * AttrPerGs);
}

float FNWItemRules::QualityMin(int32 SkillLevel, int32 MaxLevel) const
{
	// Roupas de ofício e troféus (até QualityMinAllMax) entram quando existirem.
	const float Level = MaxLevel > 0 ? FMath::Clamp(float(SkillLevel) / MaxLevel, 0.f, 1.f) : 0.f;
	return QualityMinCraftMax * Level;
}

FNWItem FNWItemRules::Roll(FName ItemId, int32 RollMin, int32 RollMax, float QualMin, const FString& Crafter) const
{
	FNWItem Out;
	Out.ItemId = ItemId;
	Out.Quantity = 1;
	const FNWItemDef* D = FNWItemCatalog::Get().Find(ItemId);
	if (!D || EquipClass(*D).IsEmpty())
	{
		return Out;
	}
	Out.GearScore = FMath::Min(FMath::RandRange(FMath::Min(RollMin, RollMax), FMath::Max(RollMin, RollMax)), GsMax);
	// Um atributo por item; provisório: sorteado entre os 5 (docs/itens.md).
	if (Attributes.Num() > 0 && EquipClass(*D) != TEXT("ferramenta"))
	{
		Out.Attr = uint8(FMath::RandRange(0, Attributes.Num() - 1));
	}
	Out.Quality = FMath::RoundToFloat(FMath::FRandRange(FMath::Clamp(QualMin, 0.f, 100.f), 100.f) * 10.f) / 10.f;
	TArray<FName> Pool = PerkPool(*D);
	const int32 N = EquipClass(*D) == TEXT("anel") ? PerksPerRing : PerksPerItem;
	// Sem repetir; no máximo AbilityPerksMax de habilidade (a da arma) por item.
	const TArray<FName>* AbilityList = Ability.Find(D->Set);
	int32 AbilityCount = 0;
	for (int32 i = 0; i < N && Pool.Num() > 0; ++i)
	{
		const int32 Pick = FMath::RandRange(0, Pool.Num() - 1);
		const FName P = Pool[Pick];
		Out.Perks.Add(P);
		Pool.RemoveAtSwap(Pick);
		if (AbilityList && AbilityList->Contains(P) && ++AbilityCount >= AbilityPerksMax)
		{
			Pool.RemoveAll([AbilityList](FName X) { return AbilityList->Contains(X); });
		}
	}
	Out.Bind = 1;   // vincula ao equipar
	if (Out.Quality >= SignQuality)
	{
		Out.Signer = Crafter;
	}
	// Raridade pelo número de perks (o tooltip e a moldura da peça usam).
	Out.Rarity = uint8(FMath::Clamp(Out.Perks.Num(), 0, 4));
	return Out;
}
