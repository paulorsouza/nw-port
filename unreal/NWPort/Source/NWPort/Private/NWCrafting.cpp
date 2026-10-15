#include "NWCrafting.h"

#include "NWCity.h"
#include "NWRules.h"

#include "Dom/JsonObject.h"
#include "EngineUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWDevRegistry.h"
#include "NWEvents.h"
#include "NWHUD.h"
#include "NWInteractable.h"
#include "NWInventoryComponent.h"
#include "NWItems.h"
#include "NWItemRules.h"
#include "NWPlayerController.h"
#include "NWProgressionComponent.h"
#include "NWText.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

int32 FNWRecipe::XpFor(int32 Count, bool bFirst) const
{
	int32 Units = 0;
	for (const FIngredient& I : Ingredients)
	{
		Units += I.Qty;
	}
	const double Base = double(Xp) * Units * Count;
	// craftingstatspanel.lua: base + floor(base × (mult − 1)) quando a conquista de primeira confecção não foi obtida.
	const bool bBonus = bFirst && bFirstCraft && FNWRules::Num(TEXT("crafting.primeira_confeccao_ativa"), 1.0) > 0.0;
	const double Mult = bBonus ? FNWRules::Num(TEXT("crafting.primeira_confeccao_xp_mult"), 3.0) : 1.0;
	return int32(Base + FMath::FloorToDouble(Base * (Mult - 1.0)));
}

FNWCrafting& FNWCrafting::Get()
{
	static FNWCrafting Instance;
	return Instance;
}

FNWCrafting::FNWCrafting()
{
	FString Raw;
	TSharedPtr<FJsonObject> Root;
	const FString File = FPaths::ProjectDir() / TEXT("Data/Crafting/refining.json");
	if (!FFileHelper::LoadFileToString(Raw, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Refino: não li %s"), *File);
		return;
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("stations")))
	{
		const TSharedPtr<FJsonObject> S = V->AsObject();
		FStation& St = StationList.AddDefaulted_GetRef();
		St.Type = S->GetStringField(TEXT("type"));
		St.Skill = FName(*S->GetStringField(TEXT("skill")));
		St.Tier = int32(S->GetNumberField(TEXT("tier")));
		const TArray<TSharedPtr<FJsonValue>>& P = S->GetArrayField(TEXT("pos"));
		St.Pos = FVector(P[0]->AsNumber(), P[1]->AsNumber(), P[2]->AsNumber());
		St.Yaw = float(S->GetNumberField(TEXT("yaw")));
		for (const auto& N : S->GetObjectField(TEXT("name"))->Values)
		{
			St.Name.Add(*N.Key, N.Value->AsString());
		}
	}
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("recipes")))
	{
		const TSharedPtr<FJsonObject> R = V->AsObject();
		FNWRecipe& Rec = Recipes.AddDefaulted_GetRef();
		Rec.Id = FName(*R->GetStringField(TEXT("id")));
		Rec.Skill = FName(*R->GetStringField(TEXT("skill")));
		Rec.Level = int32(R->GetNumberField(TEXT("level")));
		Rec.Station = R->GetStringField(TEXT("station"));
		Rec.Tier = int32(R->GetNumberField(TEXT("tier")));
		Rec.Out = FName(*R->GetStringField(TEXT("out")));
		Rec.Qty = int32(R->GetNumberField(TEXT("qty")));
		Rec.Fee = int32(R->GetNumberField(TEXT("fee")));
		Rec.Xp = int32(R->GetNumberField(TEXT("xp")));
		Rec.Group = R->GetStringField(TEXT("group"));
		R->TryGetStringField(TEXT("category"), Rec.Category);
		R->TryGetNumberField(TEXT("category_order"), Rec.CategoryOrder);
		R->TryGetNumberField(TEXT("group_order"), Rec.GroupOrder);
		R->TryGetBoolField(TEXT("first_craft"), Rec.bFirstCraft);
		R->TryGetBoolField(TEXT("gear"), Rec.bGear);
		R->TryGetNumberField(TEXT("item_tier"), Rec.ItemTier);
		R->TryGetNumberField(TEXT("gs_base"), Rec.GsBase);
		for (const TPair<const TCHAR*, TMap<FString, FString>*>& N : {TPair<const TCHAR*, TMap<FString, FString>*>(TEXT("category_name"), &Rec.CategoryName),
			TPair<const TCHAR*, TMap<FString, FString>*>(TEXT("group_name"), &Rec.GroupName)})
		{
			const TSharedPtr<FJsonObject>* O = nullptr;
			if (R->TryGetObjectField(N.Key, O))
			{
				for (const auto& L : (*O)->Values)
				{
					N.Value->Add(*L.Key, L.Value->AsString());
				}
			}
		}
		Rec.Bonus = float(R->GetNumberField(TEXT("bonus")));
		for (const TSharedPtr<FJsonValue>& V2 : R->GetArrayField(TEXT("inc")))
		{
			Rec.BonusInc.Add(float(V2->AsNumber()));
		}
		for (const TSharedPtr<FJsonValue>& V2 : R->GetArrayField(TEXT("dec")))
		{
			Rec.BonusDec.Add(float(V2->AsNumber()));
		}
		for (const TSharedPtr<FJsonValue>& I : R->GetArrayField(TEXT("ing")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = I->AsArray();
			Rec.Ingredients.Add({FName(*A[0]->AsString()), int32(A[1]->AsNumber()), A[2]->AsBool()});
		}
	}
	for (const auto& C : Root->GetObjectField(TEXT("categories"))->Values)
	{
		TArray<FName>& Members = Categories.Add(FName(*C.Key));
		for (const TSharedPtr<FJsonValue>& M : C.Value->AsArray())
		{
			Members.Add(FName(*M->AsString()));
		}
	}
}

const FNWRecipe* FNWCrafting::Find(FName Id) const
{
	return Recipes.FindByPredicate([Id](const FNWRecipe& R) { return R.Id == Id; });
}

TArray<const FNWRecipe*> FNWCrafting::ForStation(const FString& Type, int32 Tier) const
{
	TArray<const FNWRecipe*> Out;
	for (const FNWRecipe& R : Recipes)
	{
		if (R.Station == Type && R.Tier <= Tier)
		{
			Out.Add(&R);
		}
	}
	return Out;
}

const TArray<FName>& FNWCrafting::Category(FName Id) const
{
	static const TArray<FName> Empty;
	const TArray<FName>* C = Categories.Find(Id);
	return C ? *C : Empty;
}

int32 FNWCrafting::Have(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const
{
	if (!Inv)
	{
		return 0;
	}
	if (!I.bCategory)
	{
		return Inv->CountOf(I.Id);
	}
	int32 N = 0;
	for (FName M : Category(I.Id))
	{
		N += Inv->CountOf(M);
	}
	return N;
}

int32 FNWCrafting::MaxCount(const UNWInventoryComponent* Inv, const FNWRecipe& R) const
{
	int32 Max = 10000;
	for (const FNWRecipe::FIngredient& I : R.Ingredients)
	{
		Max = FMath::Min(Max, I.Qty > 0 ? Have(Inv, I) / I.Qty : Max);
	}
	if (R.Fee > 0 && Inv)
	{
		Max = FMath::Min<int64>(Max, Inv->Coins / R.Fee);
	}
	return FMath::Max(0, Max);
}

FName FNWCrafting::ChosenFor(const UNWInventoryComponent* Inv, const FNWRecipe::FIngredient& I) const
{
	if (!I.bCategory)
	{
		return I.Id;
	}
	FName Best;
	int32 Most = -1;
	for (FName M : Category(I.Id))
	{
		const int32 N = Inv ? Inv->CountOf(M) : 0;
		if (N > Most)
		{
			Most = N;
			Best = M;
		}
	}
	return Best;
}

float FNWCrafting::BonusChance(const FNWRecipe& R, int32 RollBonus, const TArray<FName>& Chosen) const
{
	// crafting.bonus_chance (C): max(0, f + roll * 0,0001). f = BonusItemChance + ajuste de tier dos
	// reagentes de categoria contra o tier do ingrediente 0 (crafting.bonus_tier_ajuste).
	float F = R.Bonus;
	auto TierOf = [](FName Id) { const FNWItemDef* D = FNWItemCatalog::Get().Find(Id); return D ? D->Tier : 0; };
	if (R.Ingredients.Num() > 0 && Chosen.Num() == R.Ingredients.Num() && R.BonusInc.Num() > 0)
	{
		const int32 Ref = TierOf(Chosen[0]);
		for (int32 i = 1; i < R.Ingredients.Num(); ++i)
		{
			if (!R.Ingredients[i].bCategory || Chosen[i].IsNone())
			{
				continue;
			}
			const int32 D = TierOf(Chosen[i]) - Ref;
			if (D > 0)
			{
				F += R.BonusInc[FMath::Min(D, R.BonusInc.Num()) - 1];
			}
			else if (D < 0 && R.BonusDec.Num() > 0)
			{
				F += R.BonusDec[FMath::Min(-D, R.BonusDec.Num()) - 1];
			}
		}
	}
	return FMath::Max(0.f, F + RollBonus * 0.0001f);   // crafting.roll_bonus_escala
}

FString FNWCrafting::Refine(ANWCharacter* Who, const FString& StationType, FName RecipeId, int32 Count, FNWCityTaxNote* Tax) const
{
	if (!Who || !Who->Inventory || !Who->Progression)
	{
		return TEXT("nw.craft.missing");
	}
	const FNWRecipe* R = Find(RecipeId);
	const ANWStation* Station = nullptr;
	for (TActorIterator<ANWStation> It(Who->GetWorld()); It; ++It)
	{
		if (It->Type == StationType && FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation()) < 500.f)
		{
			Station = *It;
			break;
		}
	}
	if (!Station || !R || R->Station != StationType || R->Tier > Station->Tier)
	{
		return TEXT("nw.craft.too_far");
	}
	Count = FMath::Clamp(Count, 1, 10000);
	if (Who->Progression->MissingTradeLevel(R->Skill, R->Level) > 0)
	{
		return TEXT("nw.craft.level");
	}
	if (MaxCount(Who->Inventory, *R) < Count)
	{
		return TEXT("nw.craft.missing");
	}
	// Item extra: chance calculada antes de gastar (os reagentes escolhidos são os que o refino vai usar).
	TArray<FName> Chosen;
	for (const FNWRecipe::FIngredient& I : R->Ingredients)
	{
		Chosen.Add(ChosenFor(Who->Inventory, I));
	}
	// Cidade da mesa (governança): imposto sobre os ingredientes e buffs da semana (item extra, XP).
	const FString Region = NWCity::RegionOf(Station->City);
	NWCity::RefreshIfOld(Who->GetWorld(), Region);
	const FNWCityState* City = Region.IsEmpty() ? nullptr : NWCity::Cached(Region);
	TMap<FName, int32> Spent;
	for (int32 i = 0; i < R->Ingredients.Num(); ++i)
	{
		Spent.FindOrAdd(Chosen[i]) += R->Ingredients[i].Qty * Count;
	}
	const int64 CityTax = Region.IsEmpty() ? 0 : NWCity::TaxFor(Region, Spent);
	if (Who->Inventory->Coins < int64(R->Fee) * Count + CityTax)
	{
		return TEXT("nw.city.no_coins");
	}
	const NWCity::FRules& CR = NWCity::Rules();
	float Chance = BonusChance(*R, Who->Progression->TradeRollBonus(NWTradeskill::Index(R->Skill)), Chosen);
	if (City && City->HasBuff(StationType, TEXT("extra")))
	{
		Chance += CR.BuffValue.FindRef(TEXT("extra")) / 100.f;
	}
	int32 Extra = 0;
	for (int32 i = 0; i < Count; ++i)
	{
		// crafting.bonus_extra_qtd (H, servidor): floor(chance) garantidos + 1 com a parte fracionária.
		Extra += FMath::FloorToInt(Chance) + (FMath::FRand() < FMath::Frac(Chance) ? 1 : 0);
	}
	for (const FNWRecipe::FIngredient& I : R->Ingredients)
	{
		int32 Need = I.Qty * Count;
		if (!I.bCategory)
		{
			Who->Inventory->Take(I.Id, Need);
			continue;
		}
		// Categoria: gasta primeiro o item de que se tem mais.
		TArray<FName> Members = Category(I.Id);
		Members.Sort([Inv = Who->Inventory.Get()](FName A, FName B) { return Inv->CountOf(A) > Inv->CountOf(B); });
		for (FName M : Members)
		{
			Need -= Who->Inventory->Take(M, Need);
		}
	}
	FNWItem Out;
	Out.ItemId = R->Out;
	Out.Quantity = R->Qty * (Count + Extra);
	Who->Inventory->Grant({Out}, -int64(R->Fee) * Count - CityTax);
	int32 RefineXp = R->XpFor(Count, !Who->Progression->HasCrafted(R->Id));   // crafting.xp_oficio
	if (City && City->HasBuff(StationType, TEXT("xp")))
	{
		RefineXp = FMath::RoundToInt(RefineXp * (1.f + CR.BuffValue.FindRef(TEXT("xp")) / 100.f));
	}
	Who->Progression->AddTradeXp(R->Skill, RefineXp);
	if (Tax && !Region.IsEmpty())
	{
		*Tax = {Region, StationType, CityTax};
	}
	Who->Progression->MarkCrafted(R->Id);
	FNWEvents::Get().OnRefine.Broadcast(Who, StationType, RecipeId, Count);
	return FString();
}

const ANWStation* FNWCrafting::NearStation(const ANWCharacter* Who, const FString& StationType) const
{
	for (TActorIterator<ANWStation> It(Who->GetWorld()); It; ++It)
	{
		if (It->Type == StationType && FVector::Dist2D(It->GetActorLocation(), Who->GetActorLocation()) < 500.f)
		{
			return *It;
		}
	}
	return nullptr;
}

TArray<FNWItem> FNWCrafting::Spend(ANWCharacter* Who, const FNWRecipe& R, int32 Count) const
{
	TArray<FNWItem> Spent;
	auto Took = [&Spent](FName Id, int32 N)
	{
		if (N > 0)
		{
			FNWItem I;
			I.ItemId = Id;
			I.Quantity = N;
			Spent.Add(I);
		}
	};
	for (const FNWRecipe::FIngredient& I : R.Ingredients)
	{
		int32 Need = I.Qty * Count;
		if (!I.bCategory)
		{
			Took(I.Id, Who->Inventory->Take(I.Id, Need));
			continue;
		}
		TArray<FName> Members = Category(I.Id);
		Members.Sort([Inv = Who->Inventory.Get()](FName A, FName B) { return Inv->CountOf(A) > Inv->CountOf(B); });
		for (FName M : Members)
		{
			const int32 N = Who->Inventory->Take(M, Need);
			Took(M, N);
			Need -= N;
		}
	}
	return Spent;
}

FString FNWCrafting::Craft(ANWCharacter* Who, const FString& StationType, FName RecipeId, FNWItem* OutItem, FNWCityTaxNote* Tax) const
{
	if (!Who || !Who->Inventory || !Who->Progression)
	{
		return TEXT("nw.craft.missing");
	}
	const FNWRecipe* R = Find(RecipeId);
	const ANWStation* Station = NearStation(Who, StationType);
	if (!Station || !R || !R->bGear || R->Station != StationType || R->Tier > Station->Tier)
	{
		return TEXT("nw.craft.too_far");
	}
	if (Who->Progression->MissingTradeLevel(R->Skill, R->Level) > 0)
	{
		return TEXT("nw.craft.level");
	}
	if (MaxCount(Who->Inventory, *R) < 1)
	{
		return TEXT("nw.craft.missing");
	}
	const FNWItemRules& Rules = FNWItemRules::Get();
	const int32 Skill = NWTradeskill::Index(R->Skill);
	const int32 Level = Who->Progression->TradeLevel(Skill);
	// Cidade da mesa (governança): imposto sobre os ingredientes, nível da mesa (GS e qualidade mínima) e buff de XP.
	const FString Region = NWCity::RegionOf(Station->City);
	NWCity::RefreshIfOld(Who->GetWorld(), Region);
	const FNWCityState* City = Region.IsEmpty() ? nullptr : NWCity::Cached(Region);
	const NWCity::FRules& CR = NWCity::Rules();
	int32 GsUp = 0;
	float QualUp = 0.f;
	NWCity::StationBonus(City, StationType, GsUp, QualUp);
	TMap<FName, int32> Spent;
	for (const FNWRecipe::FIngredient& I : R->Ingredients)
	{
		Spent.FindOrAdd(I.bCategory ? ChosenFor(Who->Inventory, I) : I.Id) += I.Qty;
	}
	const int64 CityTax = Region.IsEmpty() ? 0 : NWCity::TaxFor(Region, Spent);
	if (Who->Inventory->Coins < int64(R->Fee) + CityTax)
	{
		return TEXT("nw.city.no_coins");
	}
	int32 Xp = R->XpFor(1, !Who->Progression->HasCrafted(R->Id));
	if (City && City->HasBuff(StationType, TEXT("xp")))
	{
		Xp = FMath::RoundToInt(Xp * (1.f + CR.BuffValue.FindRef(TEXT("xp")) / 100.f));
	}
	const bool bTraining = Level < Rules.CraftLevel;
	// Bolsa cheia: o item vai para o chão (UNWInventoryComponent::Grant), não cancela a fabricação.
	Spend(Who, *R, 1);
	FNWItem Out;
	if (!bTraining)
	{
		const FIntPoint Range = GsRange(Who, *R);
		FString Crafter = Who->GetName();
		if (const ANWPlayerController* PC = Cast<ANWPlayerController>(Who->GetController()); PC && PC->Record)
		{
			Crafter = PC->Record->Name;
		}
		// Mesa evoluída: um pouco de GS e de qualidade mínima por nível (docs/game/economy.md).
		const float QualMin = FMath::Min(100.f, Rules.QualityMin(Level, Who->Progression->TradeMaxLevel(Skill)) + QualUp);
		Out = Rules.Roll(R->Out, FMath::Min(Range.X + GsUp, Rules.GsMax), FMath::Min(Range.Y + GsUp, Rules.GsMax), QualMin, Crafter);
	}
	// Treino (abaixo do nível de fabricar): só XP, sem item. Com OutItem quem chamou põe na bolsa (e pede o número de
	// série do item assinado ao nw-core).
	const bool bHold = !bTraining && OutItem;
	if (bHold)
	{
		*OutItem = Out;
	}
	Who->Inventory->Grant(bTraining || bHold ? TArray<FNWItem>() : TArray<FNWItem>{Out}, -int64(R->Fee) - CityTax);
	if (Tax && !Region.IsEmpty())
	{
		*Tax = {Region, StationType, CityTax};
	}
	Who->Progression->AddTradeXp(R->Skill, Xp);
	Who->Progression->MarkCrafted(R->Id);
	return FString();
}

const FNWRecipe* FNWCrafting::RecipeFor(FName ItemId, const FString& StationType) const
{
	return Recipes.FindByPredicate([&](const FNWRecipe& R) { return R.bGear && R.Out == ItemId && R.Station == StationType; });
}

FIntPoint FNWCrafting::GsRange(const ANWCharacter* Who, const FNWRecipe& R) const
{
	// Faixa do item do catálogo mais o bônus do artesão pelo nível (tabela do ofício do jogo), com o teto do port.
	const FNWItemDef* D = FNWItemCatalog::Get().Find(R.Out);
	const FIntPoint Bonus = Who && Who->Progression ? Who->Progression->TradeGsBonus(NWTradeskill::Index(R.Skill), R.ItemTier) : FIntPoint::ZeroValue;
	const int32 Cap = FNWItemRules::Get().GsMax;
	const int32 Min = (D && D->GearScoreMin > 0 ? D->GearScoreMin : R.GsBase) + Bonus.X;
	const int32 Max = (D && D->GearScoreMax > 0 ? D->GearScoreMax : R.GsBase) + Bonus.Y;
	return FIntPoint(FMath::Min(Min, Cap), FMath::Min(FMath::Max(Min, Max), Cap));
}

TArray<FName> FNWCrafting::ImproveMaterials(const FNWRecipe& R) const
{
	TArray<FName> Out;
	for (const FNWItemRules::FImproveMaterial& M : FNWItemRules::Get().ImproveMaterials)
	{
		const bool bRecipe = R.Ingredients.IsValidIndex(M.RecipeIndex) && !R.Ingredients[M.RecipeIndex].bCategory;
		Out.Add(!M.Item.IsNone() ? M.Item : bRecipe ? R.Ingredients[M.RecipeIndex].Id : FName());
	}
	return Out;
}

FNWRecipe::FIngredient FNWCrafting::ImproveCost(const FNWRecipe& R, const FNWItem& Item, ENWImprove Kind, uint8 Mat) const
{
	FNWRecipe::FIngredient Cost;
	const FNWItemRules& Rules = FNWItemRules::Get();
	const TArray<FName> Mats = ImproveMaterials(R);
	if (!Mats.IsValidIndex(Mat) || Mats[Mat].IsNone())
	{
		return Cost;
	}
	Cost.Id = Mats[Mat];
	switch (Kind)
	{
	case ENWImprove::Attribute:
		Cost.Qty = Item.Attr != 255 && Rules.Attributes.Num() > 1 ? Rules.CostAttr : 0;
		break;
	case ENWImprove::GearScore:
		Cost.Qty = Item.Quality >= 0.f ? Rules.CostGs : 0;
		break;
	case ENWImprove::Unbind:
		Cost.Qty = Item.Bind == 2 ? Rules.CostUnbind + FMath::FloorToInt(FMath::Max(0.f, Item.Quality) / FMath::Max(1.f, Rules.UnbindQualityPer)) : 0;
		break;
	default:
		break;
	}
	return Cost;
}

FNWCrafting::FGsOdds FNWCrafting::GsOdds(const ANWCharacter* Who, const FNWRecipe& R, const FNWItem& Item, uint8 Mat) const
{
	// Tetos de subida e descida (docs/game/items.md "Gear score"): quanto mais acima do mínimo da faixa, menor o teto de subir;
	// nunca sai da faixa da fabricação.
	const FNWItemRules& Rules = FNWItemRules::Get();
	const TArray<FNWItemRules::FGsStep>& Steps = Rules.ImproveMaterials.IsValidIndex(Mat) && Rules.ImproveMaterials[Mat].Steps.Num()
		? Rules.ImproveMaterials[Mat].Steps : Rules.GsSteps;
	const FIntPoint Range = GsRange(Who, R);
	const int32 Cur = Item.GearScore;
	FIntPoint Step(0, 0);
	for (const FNWItemRules::FGsStep& St : Steps)
	{
		Step = FIntPoint(St.Min, St.Max);
		if (Cur - Range.X <= St.UpTo)
		{
			break;
		}
	}
	FGsOdds Odds;
	Odds.UpMax = FMath::Clamp(Range.Y - Cur, 0, FMath::Max(0, Step.Y));
	Odds.DownMax = FMath::Clamp(Cur - Range.X, 0, FMath::Max(0, -Step.X));
	Odds.Range = FIntPoint(Cur - Odds.DownMax, Cur + Odds.UpMax);
	return Odds;
}

namespace
{
	/** 1..N com 1 mais provável que 2 e assim por diante (pesos N, N−1, ..., 1); 0 se N = 0. */
	int32 RollStep(int32 N)
	{
		if (N <= 0)
		{
			return 0;
		}
		int32 Pick = FMath::RandRange(1, N * (N + 1) / 2);
		for (int32 k = 1; k <= N; ++k)
		{
			Pick -= N + 1 - k;
			if (Pick <= 0)
			{
				return k;
			}
		}
		return N;
	}
}

FString FNWCrafting::Improve(ANWCharacter* Who, const FString& StationType, int32 BagSlot, ENWImprove Kind, uint8 Mat, FNWItem* Before,
	FNWItem* After) const
{
	if (!Who || !Who->Inventory || !NearStation(Who, StationType))
	{
		return TEXT("nw.craft.too_far");
	}
	auto At = [Who, BagSlot]() { return Who->Inventory->Items.FindByPredicate([BagSlot](const FNWItem& I) { return I.Container == ENWItemContainer::Bag && I.Slot == BagSlot; }); };
	FNWItem* Item = At();
	const FNWRecipe* R = Item ? RecipeFor(Item->ItemId, StationType) : nullptr;
	if (!R)
	{
		return TEXT("nw.craft.missing");
	}
	const FNWRecipe::FIngredient Cost = ImproveCost(*R, *Item, Kind, Mat);
	if (Cost.Qty <= 0 || Who->Inventory->CountOf(Cost.Id) < Cost.Qty)
	{
		return TEXT("nw.craft.missing");
	}
	Who->Inventory->Take(Cost.Id, Cost.Qty);
	Item = At();   // o Take pode ter mexido no array
	if (!Item)
	{
		return TEXT("nw.craft.missing");
	}
	if (Before)
	{
		*Before = *Item;
	}
	const FNWItemRules& Rules = FNWItemRules::Get();
	switch (Kind)
	{
	case ENWImprove::Attribute:
	{
		// Outro atributo, nunca o mesmo. GS, qualidade e perks ficam.
		const int32 Next = FMath::RandRange(0, Rules.Attributes.Num() - 2);
		Item->Attr = uint8(Next >= Item->Attr ? Next + 1 : Next);
		break;
	}
	case ENWImprove::GearScore:
	{
		// Refinar: chance_sucesso_gs de subir, senão desce, de 1 até o teto (GsOdds); com o GS muda o atributo. Qualidade e
		// perks ficam.
		const FGsOdds Odds = GsOdds(Who, *R, *Item, Mat);
		Item->GearScore += FMath::FRand() * 100.f < Rules.GsSuccess ? RollStep(Odds.UpMax) : -RollStep(Odds.DownMax);
		break;
	}
	case ENWImprove::Unbind:
		Item->Bind = 1;   // volta a vincular ao equipar
		break;
	default:
		break;
	}
	if (After)
	{
		*After = *Item;
	}
	Who->ForceNetUpdate();
	return FString();
}

ANWStation::ANWStation()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Interact = CreateDefaultSubobject<UNWInteractableComponent>(TEXT("Interact"));
	Interact->ActionKey = TEXT("ui_prefab_refinery_interact");
	Interact->Radius = 350.f;
	Interact->FocusHeight = 100.f;
}

void ANWStation::Setup(const FNWCrafting::FStation& Data)
{
	Type = Data.Type;
	Tier = Data.Tier;
	const bool bStorage = Type == TEXT("storage");
	if (bStorage || Type == TEXT("tradingpost") || Type == TEXT("cityboard") || Type == TEXT("governance"))
	{
		Interact->ActionKey = TEXT("nw.storage.open");
	}
	const FString* Name = Data.Name.Find(NWText::Lang());
	Interact->DisplayName = FText::FromString(Name ? *Name : Data.Type);
	TWeakObjectPtr<ANWStation> Self(this);
	Interact->OnInteract = [Self](ANWCharacter* Who)
	{
		if (!Self.IsValid() || !Who)
		{
			return;
		}
		if (ANWPlayerController* PC = Cast<ANWPlayerController>(Who->GetController()))
		{
			if (ANWHUD* HUD = Cast<ANWHUD>(PC->GetHUD()))
			{
				if (Self->Type == TEXT("storage"))
				{
					HUD->OpenStorage(Who, Self.Get());
				}
				else if (Self->Type == TEXT("tradingpost"))
				{
					HUD->OpenMarket(Who, Self.Get());
				}
				else if (Self->Type == TEXT("cityboard") || Self->Type == TEXT("governance"))
				{
					HUD->OpenCity(Who, Self.Get(), Self->Type == TEXT("governance"));
				}
				else
				{
					HUD->OpenRefining(Who, Self.Get());
				}
			}
		}
	};
}

// Testar gemas (docs/game/items.md "Slot"): Joalheria e Cantaria no 200 (vidro rúnico pede 180), as 7 gemas Puras, 2 vidros
// rúnicos, gema bruta T4 para o custo de retirar, material para lapidar uma T1 e montar um vidro.
NW_REGISTER_DEV(DevGems, TEXT("sistemas.dev_gems"), TEXT("nw.dev.dev_gems"),
	[](ANWPlayerController* PC)
	{
		ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (!C || !C->Inventory || !C->Progression)
		{
			return;
		}
		for (const TCHAR* Skill : {TEXT("Jewelcrafting"), TEXT("Stonecutting")})
		{
			const int32 t = NWTradeskill::Index(Skill);
			for (int32 i = 0; i < 300 && C->Progression->TradeLevel(t) < 200; ++i)
			{
				C->Progression->AddTradeXp(Skill, FMath::Max(1, C->Progression->TradeXpToNext(t)));
			}
		}
		TArray<FNWItem> Gift;
		auto Give = [&Gift](const FString& Id, int32 Qty)
		{
			if (FNWItemCatalog::Get().Find(FName(*Id)))
			{
				FNWItem I;
				I.ItemId = FName(*Id);
				I.Quantity = Qty;
				Gift.Add(I);
			}
		};
		for (const TCHAR* Gem : {TEXT("Onyx"), TEXT("Opal"), TEXT("Diamond"), TEXT("Sapphire"), TEXT("Jasper"), TEXT("Emerald"), TEXT("Malachite")})
		{
			Give(FString(Gem) + TEXT("CutT4"), 3);
			Give(FString(Gem) + TEXT("T4"), 5);
		}
		Give(TEXT("Runeglass_Gem_Onyx_Melee"), 1);
		Give(TEXT("Runeglass_Gem_Emerald_Arcane"), 1);
		Give(TEXT("OnyxT1"), 4);
		Give(TEXT("AlchemyLifeT1"), 8);
		Give(TEXT("Runeglass_Case_Empty_Melee"), 1);
		Give(TEXT("SolventT5"), 2);
		Give(TEXT("IngotPreciousT2"), 1);
		C->Inventory->Grant(Gift, 0);
	});

// Testar armaduras e joias (docs/game/items.md): Armoraria e Joalheria no 200 (receitas T5 pedem 150) e material refinado
// T2–T5 de placa, couro e tecido, mais os lingotes preciosos das joias (prata, ouro, platina; oricalco é o IngotT5).
NW_REGISTER_DEV(DevArmor, TEXT("sistemas.dev_armor"), TEXT("nw.dev.dev_armor"),
	[](ANWPlayerController* PC)
	{
		ANWCharacter* C = PC ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		if (!C || !C->Inventory || !C->Progression)
		{
			return;
		}
		for (const TCHAR* Skill : {TEXT("Armoring"), TEXT("Jewelcrafting")})
		{
			const int32 t = NWTradeskill::Index(Skill);
			for (int32 i = 0; i < 300 && C->Progression->TradeLevel(t) < 200; ++i)
			{
				C->Progression->AddTradeXp(Skill, FMath::Max(1, C->Progression->TradeXpToNext(t)));
			}
		}
		TArray<FNWItem> Gift;
		auto Give = [&Gift](const FString& Id, int32 Qty)
		{
			if (FNWItemCatalog::Get().Find(FName(*Id)))
			{
				FNWItem I;
				I.ItemId = FName(*Id);
				I.Quantity = Qty;
				Gift.Add(I);
			}
		};
		for (int32 Tier = 2; Tier <= 5; ++Tier)
		{
			Give(FString::Printf(TEXT("IngotT%d"), Tier), 120);
			Give(FString::Printf(TEXT("LeatherT%d"), Tier), 120);
			Give(FString::Printf(TEXT("ClothT%d"), Tier), 120);
		}
		for (int32 Tier = 1; Tier <= 3; ++Tier)
		{
			Give(FString::Printf(TEXT("IngotPreciousT%d"), Tier), 40);
		}
		C->Inventory->Grant(Gift, 500000);
	});
