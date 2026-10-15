#include "NWUIShotCommandlet.h"

#include "CanvasTypes.h"
#include "ContentStreaming.h"
#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "EngineFontServices.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/ISlateRHIRendererModule.h"
#include "Engine/Engine.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "NWCity.h"
#include "NWMarket.h"
#include "SNWCity.h"
#include "SNWMarket.h"
#include "Slate/WidgetRenderer.h"
#include "Engine/World.h"
#include "NWLootBag.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCharacter.h"
#include "NWCrafting.h"
#include "NWInventoryComponent.h"
#include "NWProgressionComponent.h"
#include "NWRefiningMenu.h"
#include "NWInventoryMenu.h"
#include "NWHUD.h"
#include "NWCombatComponent.h"
#include "NWInteractable.h"
#include "NWNpcRewards.h"
#include "NWShopMenu.h"
#include "NWUILayout.h"
#include "RenderingThread.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#if WITH_EDITOR
#include "TextureCompiler.h"
#endif
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWUIShot, Log, All);

UNWUIShotCommandlet::UNWUIShotCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace
{
	// Minimum XP for the trade skill level (the tables live in the component; looks it up through TradeLevel itself).
	void SetTradeLevel(UNWProgressionComponent* P, int32 Skill, int32 Level)
	{
		int32 Lo = 0, Hi = 1;
		P->TradeXp[Skill] = Hi;
		while (P->TradeLevel(Skill) < Level && Hi < (1 << 29))
		{
			Hi *= 2;
			P->TradeXp[Skill] = Hi;
		}
		while (Lo < Hi)
		{
			const int32 Mid = Lo + (Hi - Lo) / 2;
			P->TradeXp[Skill] = Mid;
			if (P->TradeLevel(Skill) >= Level) { Hi = Mid; } else { Lo = Mid + 1; }
		}
		P->TradeXp[Skill] = Lo;
	}

	// UI textures reach the render target only after they are compiled and with all mips.
	void SettleTextures()
	{
#if WITH_EDITOR
		FTextureCompilingManager::Get().FinishAllCompilation();   // exists only in the editor (the server compiles the file too)
#endif
		for (TObjectIterator<UTexture2D> It; It; ++It)
		{
			It->SetForceMipLevelsToBeResident(30.f);
		}
		IStreamingManager::Get().StreamAllResources(5.f);
		// Runtime font glyphs: the atlas goes up to the GPU on the engine tick, which the commandlet does not have.
		if (FEngineFontServices::IsInitialized())
		{
			FEngineFontServices::Get().UpdateCache();
		}
		FlushRenderingCommands();
	}
}

int32 UNWUIShotCommandlet::Main(const FString& Params)
{
	FString File, OutDir;
	FParse::Value(*Params, TEXT("scenario="), File);
	if (!FParse::Value(*Params, TEXT("out="), OutDir))
	{
		OutDir = FPaths::ProjectDir() / TEXT("../../extract/ui/shots");
	}
	FString Text;
	TSharedPtr<FJsonObject> S;
	if (!FFileHelper::LoadFileToString(Text, *File) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), S) || !S)
	{
		UE_LOG(LogNWUIShot, Error, TEXT("invalid scenario: %s"), *File);
		return 1;
	}
	const FString Screen = S->GetStringField(TEXT("screen"));
	if (Screen != TEXT("refining") && Screen != TEXT("shop") && Screen != TEXT("inventory") && Screen != TEXT("market") && Screen != TEXT("city")
		&& Screen != TEXT("hud"))
	{
		UE_LOG(LogNWUIShot, Error, TEXT("screen without capture: %s"), *Screen);
		return 1;
	}

	// Runtime fonts measure and draw through the Slate cache, which the commandlet brings up without a renderer.
	if (!FSlateApplication::IsInitialized())
	{
		FSlateApplication::Create();
	}
	if (FSlateApplication::IsInitialized() && !FSlateApplication::Get().GetRenderer())
	{
		FSlateApplication::Get().InitializeRenderer(
			FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlateRHIRenderer(), true);
	}
	UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot slate: app %d, renderer %d"), FSlateApplication::IsInitialized(),
		FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer() != nullptr);
	if (FEngineFontServices::IsInitialized())
	{
		FEngineFontServices::Destroy();
	}
	FEngineFontServices::Create();
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("NWUIShot"));
	FWorldContext& Ctx = GEngine->CreateNewWorldContext(EWorldType::Game);
	Ctx.SetCurrentWorld(World);
	World->AddToRoot();

	// No construction (mesh, anim): the screen only reads the bag, the levels and the interaction component.
	FActorSpawnParameters Deferred;
	Deferred.bDeferConstruction = true;
	ANWCharacter* Me = World->SpawnActor<ANWCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Deferred);
	ANWStation* St = World->SpawnActor<ANWStation>(FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Me || !St)
	{
		UE_LOG(LogNWUIShot, Error, TEXT("did not create the character/station"));
		return 1;
	}
	if (S->HasField(TEXT("station")))
	{
		const TSharedPtr<FJsonObject> SO = S->GetObjectField(TEXT("station"));
		FNWCrafting::FStation Data;
		Data.Type = SO->GetStringField(TEXT("type"));
		Data.Tier = int32(SO->GetNumberField(TEXT("tier")));
		for (const FString Lang : {TEXT("pt-br"), TEXT("en-us")})
		{
			Data.Name.Add(Lang, SO->GetStringField(TEXT("name")));
		}
		St->Setup(Data);
	}

	TArray<FNWItem> Items;
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("items")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		FNWItem I;
		I.ItemId = FName(A[0]->AsString());
		I.Quantity = int32(A[1]->AsNumber());
		I.Slot = Items.Num();
		I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
		I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
		if (A.Num() > 4 && A[4]->Type == EJson::Object)
		{
			NWItemProps::Read(I, A[4]->AsObject());   // props of the crafted item (quality, perks, gem...)
		}
		Items.Add(I);
	}
	// Inventory: equipment [[item, slot, gs, rarity]] and storage [[item, qty, gs, rarity]].
	const TArray<TSharedPtr<FJsonValue>>* Equip = nullptr;
	if (S->TryGetArrayField(TEXT("equip"), Equip))
	{
		for (const TSharedPtr<FJsonValue>& V : *Equip)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			FNWItem I;
			I.ItemId = FName(A[0]->AsString());
			I.Quantity = 1;
			I.Container = ENWItemContainer::Equip;
			I.Slot = int32(A[1]->AsNumber());
			I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
			I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
			if (A.Num() > 4 && A[4]->Type == EJson::Object)
			{
				NWItemProps::Read(I, A[4]->AsObject());
			}
			Items.Add(I);
		}
	}
	TArray<FNWItem> Stored;
	const TArray<TSharedPtr<FJsonValue>>* StoredArr = nullptr;
	if (S->TryGetArrayField(TEXT("storage"), StoredArr))
	{
		for (const TSharedPtr<FJsonValue>& V : *StoredArr)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			FNWItem I;
			I.ItemId = FName(A[0]->AsString());
			I.Quantity = int32(A[1]->AsNumber());
			I.Slot = Stored.Num();
			I.GearScore = A.Num() > 2 ? int32(A[2]->AsNumber()) : 0;
			I.Rarity = A.Num() > 3 ? uint8(A[3]->AsNumber()) : 0;
			I.Tab = A.Num() > 4 ? uint8(A[4]->AsNumber()) : 0;   // storage tab
			Stored.Add(I);
		}
	}
	if (Me->Interactor)
	{
		Me->Interactor->StorageItems = Stored;
		if (S->HasField(TEXT("storage_capacity")))
		{
			Me->Interactor->StorageCapacity = float(S->GetNumberField(TEXT("storage_capacity")));
		}
	}
	Me->Inventory->Load(Items, int64(S->GetNumberField(TEXT("coins"))));
	// Trading Post: orders [{"item", "qty", "gs", "rarity", "props", "price", "seller", "mine", "hours"}].
	const TArray<TSharedPtr<FJsonValue>>* Orders = nullptr;
	if (Me->Interactor && S->TryGetArrayField(TEXT("orders"), Orders))
	{
		int32 N = 0;
		for (const TSharedPtr<FJsonValue>& V : *Orders)
		{
			const TSharedPtr<FJsonObject> O = V->AsObject();
			FNWMarketRow Row;
			Row.Id = FString::Printf(TEXT("shot-%d"), N++);
			Row.Item.ItemId = FName(O->GetStringField(TEXT("item")));
			Row.Item.Quantity = O->HasField(TEXT("qty")) ? int32(O->GetNumberField(TEXT("qty"))) : 1;
			Row.Item.GearScore = O->HasField(TEXT("gs")) ? int32(O->GetNumberField(TEXT("gs"))) : 0;
			Row.Item.Rarity = O->HasField(TEXT("rarity")) ? uint8(O->GetNumberField(TEXT("rarity"))) : 0;
			const TSharedPtr<FJsonObject>* Props = nullptr;
			if (O->TryGetObjectField(TEXT("props"), Props))
			{
				NWItemProps::Read(Row.Item, *Props);
			}
			Row.UnitPrice = int64(O->GetNumberField(TEXT("price")));
			O->TryGetStringField(TEXT("seller"), Row.Seller);
			O->TryGetBoolField(TEXT("mine"), Row.bMine);
			Row.ExpiresIn = O->HasField(TEXT("hours")) ? float(O->GetNumberField(TEXT("hours"))) * 3600.f : 72.f * 3600.f;
			Me->Interactor->MarketRows.Add(Row);
		}
	}
	// City: {"city": <response of GET /internal/regions/1/city>, "city_mine": true}.
	const TSharedPtr<FJsonObject>* CityJson = nullptr;
	if (Me->Interactor && S->TryGetObjectField(TEXT("city"), CityJson))
	{
		bool bMineCity = false;
		S->TryGetBoolField(TEXT("city_mine"), bMineCity);
		FString Owner;
		(*CityJson)->TryGetStringField(TEXT("owner_id"), Owner);
		NWCity::Parse(*CityJson, bMineCity ? Owner : FString(), Me->Interactor->CityState);
	}
	Me->Progression->bFromBackend = true;   // requires the trade skill level, as in the game with a backend
	// Shop: purchases already made this week (weekly limit), {"FluxT5": 100}.
	const TSharedPtr<FJsonObject>* Bought = nullptr;
	if (S->TryGetObjectField(TEXT("bought"), Bought))
	{
		for (const auto& B : (*Bought)->Values)
		{
			Me->Progression->AddShopBought(FName(*FString(B.Key)), int32(B.Value->AsNumber()), FNWNpcRewards::Get().Period());
		}
	}
	const TSharedPtr<FJsonObject>* Trade = nullptr;
	const TSharedPtr<FJsonObject> NoTrade = MakeShared<FJsonObject>();
	for (const auto& T : (S->TryGetObjectField(TEXT("trade"), Trade) ? *Trade : NoTrade)->Values)
	{
		const int32 Skill = NWTradeskill::Index(FName(*FString(T.Key)));
		if (Skill != INDEX_NONE)
		{
			SetTradeLevel(Me->Progression, Skill, int32(T.Value->AsNumber()));
		}
	}

	FString Only;
	FParse::Value(*Params, TEXT("shot="), Only);
	const FString Base = FPaths::GetBaseFilename(File);
	int32 Saved = 0;
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("shots")))
	{
		const TSharedPtr<FJsonObject> Shot = V->AsObject();
		const FString Name = Shot->GetStringField(TEXT("name"));
		if (!Only.IsEmpty() && Only != Name)
		{
			continue;
		}
		const int32 W = Shot->HasField(TEXT("w")) ? int32(Shot->GetNumberField(TEXT("w"))) : 1920;
		const int32 H = Shot->HasField(TEXT("h")) ? int32(Shot->GetNumberField(TEXT("h"))) : 1080;
		FVector2D Mouse(-100.0, -100.0);
		const TArray<TSharedPtr<FJsonValue>>* MouseArr = nullptr;
		if (Shot->TryGetArrayField(TEXT("mouse"), MouseArr) && MouseArr->Num() == 2)
		{
			Mouse = FVector2D((*MouseArr)[0]->AsNumber(), (*MouseArr)[1]->AsNumber());
		}

		UTextureRenderTarget2D* RT = NewObject<UTextureRenderTarget2D>();
		RT->RenderTargetFormat = RTF_RGBA8_SRGB;
		RT->ClearColor = FLinearColor(FColor(0x1E, 0x1C, 0x19));   // dark neutral background (in the game, the world behind)
		RT->InitAutoFormat(W, H);
		RT->UpdateResourceImmediate(true);
		FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();

		Me->Inventory->Load(Items, int64(S->GetNumberField(TEXT("coins"))));   // the scenario's bag on every capture (the roll really refines)
		FNWRefiningMenu Menu;
		FNWShopMenu Shop;
		FNWInventoryMenu Inventory;
		const bool bShop = Screen == TEXT("shop");
		const bool bMarket = Screen == TEXT("market");
		TSharedPtr<SNWMarket> Market;
		TSharedPtr<SNWCity> CityMenu;
		const bool bInventory = Screen == TEXT("inventory");
		const bool bHud = Screen == TEXT("hud");
		Menu.FixedNow = Shop.FixedNow = 1000.0;
		// Combat HUD: {"health": 0..1, "stamina": 0..1, "combat": bool, "active": set, "loadout": [a, b], "yaw": deg,
		// "map": "L_SouthWindsward", "loot": [[item, qty]], "xp": n}.
		static ANWHUD* Hud = nullptr;
		bool bHudCombat = false;
		float HudYaw = 0.f;
		FString HudMap;
		if (bHud)
		{
			if (!Hud)
			{
				Hud = World->SpawnActor<ANWHUD>();
			}
			UNWCombatComponent* C = Me->Combat;
			C->Health = C->MaxHealth * (Shot->HasField(TEXT("health")) ? float(Shot->GetNumberField(TEXT("health"))) : 1.f);
			C->Stamina = C->MaxStamina * (Shot->HasField(TEXT("stamina")) ? float(Shot->GetNumberField(TEXT("stamina"))) : 1.f);
			Shot->TryGetBoolField(TEXT("combat"), bHudCombat);
			HudYaw = Shot->HasField(TEXT("yaw")) ? float(Shot->GetNumberField(TEXT("yaw"))) : 0.f;
			Shot->TryGetStringField(TEXT("map"), HudMap);
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (Shot->TryGetArrayField(TEXT("loadout"), Pair) && Pair->Num() == 2)
			{
				Me->Loadout = {int32((*Pair)[0]->AsNumber()), int32((*Pair)[1]->AsNumber())};
			}
			Me->ActiveWeapon = Shot->HasField(TEXT("active")) ? int32(Shot->GetNumberField(TEXT("active"))) : Me->Loadout[0];
			if (Me->WeaponSets.IsValidIndex(Me->ActiveWeapon))
			{
				Me->Abilities = Me->WeaponSets[Me->ActiveWeapon].Abilities;
			}
			const TArray<TSharedPtr<FJsonValue>>* Loot = nullptr;
			if (Shot->TryGetArrayField(TEXT("loot"), Loot))
			{
				TArray<FNWItem> Gained;
				for (const TSharedPtr<FJsonValue>& LV : *Loot)
				{
					FNWItem I;
					I.ItemId = FName(LV->AsArray()[0]->AsString());
					I.Quantity = int32(LV->AsArray()[1]->AsNumber());
					Gained.Add(I);
				}
				Hud->ShowLoot(Gained, 0);
			}
		}
		const int32 Count = Shot->HasField(TEXT("count")) ? int32(Shot->GetNumberField(TEXT("count"))) : 1;
		if (bInventory)
		{
			bool bStorageShot = false, bLootShot = false;
			Shot->TryGetBoolField(TEXT("storage"), bStorageShot);
			Shot->TryGetBoolField(TEXT("loot"), bLootShot);
			if (bLootShot)
			{
				// Bag on the ground with the "storage" items (loot bag screen).
				ANWLootBag* Bag = World->SpawnActor<ANWLootBag>(FVector::ZeroVector, FRotator::ZeroRotator);
				if (Bag)
				{
					Bag->Items = Stored;
				}
				Inventory.OpenLootBag(Me, Bag);
			}
			else
			{
				bStorageShot ? Inventory.OpenStorage(Me, S->GetStringField(TEXT("storage_title"))) : Inventory.Open(Me);
			}
			if (Shot->HasField(TEXT("storage_tab")))
			{
				Inventory.ShotTab(int32(Shot->GetNumberField(TEXT("storage_tab"))));
			}
			const TArray<TSharedPtr<FJsonValue>>* TabNames = nullptr;
			if (S->TryGetArrayField(TEXT("storage_tab_names"), TabNames) && Me->Interactor)
			{
				Me->Interactor->StorageTabNames.Reset();
				for (const TSharedPtr<FJsonValue>& TabName : *TabNames)
				{
					Me->Interactor->StorageTabNames.Add(TabName->AsString());
				}
			}
			// Sorting: {"sort": 1..4} on the inventory list, {"storage_sort": 1..4} on the storage list.
			if (Shot->HasField(TEXT("sort")))
			{
				Inventory.ShotSort(0, int32(Shot->GetNumberField(TEXT("sort"))));
			}
			if (Shot->HasField(TEXT("storage_sort")))
			{
				Inventory.ShotSort(1, int32(Shot->GetNumberField(TEXT("storage_sort"))));
			}
			// Search: {"search": text} in the bag, {"storage_search": text} in storage.
			FString Query;
			if (Shot->TryGetStringField(TEXT("search"), Query))
			{
				Inventory.ShotSearch(0, Query);
			}
			if (Shot->TryGetStringField(TEXT("storage_search"), Query))
			{
				Inventory.ShotSearch(1, Query);
			}
			// Popup: {"popup": {"kind": "split|destroy|socket|unsocket", "from": "bag|equip|storage", "item": Id, "qty": n,
			// "gem": Id (socket: gem in the bag)}}.
			const TSharedPtr<FJsonObject>* Pop = nullptr;
			if (Shot->TryGetObjectField(TEXT("popup"), Pop))
			{
				const FString From = (*Pop)->GetStringField(TEXT("from"));
				const int32 Src = From == TEXT("storage") ? 2 : From == TEXT("equip") ? 1 : 0;
				const FName Id((*Pop)->GetStringField(TEXT("item")));
				const TArray<FNWItem>& Pool = Src == 2 ? Me->Interactor->StorageItems : Me->Inventory->Items;
				const ENWItemContainer Cont = Src == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag;
				if (const FNWItem* Found = Pool.FindByPredicate([&](const FNWItem& I) { return I.ItemId == Id && (Src == 2 || I.Container == Cont); }))
				{
					const FString Kind = (*Pop)->GetStringField(TEXT("kind"));
					int32 Qty = (*Pop)->HasField(TEXT("qty")) ? int32((*Pop)->GetNumberField(TEXT("qty"))) : 0;
					FString Gem;
					if (Kind == TEXT("socket") && (*Pop)->TryGetStringField(TEXT("gem"), Gem))
					{
						const FNWItem* G = Me->Inventory->Items.FindByPredicate([&](const FNWItem& I) { return I.ItemId == FName(*Gem) && I.Container == ENWItemContainer::Bag; });
						Qty = G ? G->Slot : INDEX_NONE;
					}
					const int32 K = Kind == TEXT("destroy") ? 2 : Kind == TEXT("socket") ? 3 : Kind == TEXT("unsocket") ? 4 : 1;
					Inventory.ShotPopup(K, Src, Found->Slot, Qty);
				}
			}
			// Simulated drag: {"from": "bag|equip|storage", "item": Id}.
			const TSharedPtr<FJsonObject>* Drag = nullptr;
			if (Shot->TryGetObjectField(TEXT("drag"), Drag))
			{
				const FString From = (*Drag)->GetStringField(TEXT("from"));
				const FName Id((*Drag)->GetStringField(TEXT("item")));
				const int32 Src = From == TEXT("equip") ? 1 : From == TEXT("storage") ? 2 : 0;
				const TArray<FNWItem>& Pool = Src == 2 ? Me->Interactor->StorageItems : Me->Inventory->Items;
				const FNWItem* Found = Pool.FindByPredicate([&](const FNWItem& I)
				{
					return I.ItemId == Id && (Src == 2 || I.Container == (Src == 1 ? ENWItemContainer::Equip : ENWItemContainer::Bag));
				});
				if (Found)
				{
					Inventory.ShotDrag(Src, Found->Slot);
				}
			}
		}
		else if (Screen == TEXT("city"))
		{
			// {"governance": bool, "tab": n, "select": Item}.
			bool bGov = false;
			Shot->TryGetBoolField(TEXT("governance"), bGov);
			CityMenu = SNew(SNWCity).Character(Me).Title(S->GetStringField(TEXT("title"))).bGovernance(bGov).bShot(true);
			CityMenu->ShotTab(Shot->HasField(TEXT("tab")) ? int32(Shot->GetNumberField(TEXT("tab"))) : 0);
			FString Sel;
			if (Shot->TryGetStringField(TEXT("select"), Sel))
			{
				CityMenu->ShotSelect(FName(Sel));
			}
		}
		else if (bMarket)
		{
			// Slate screen: {"tab": 0..2, "search": text, "perk": Id, "select": row index}.
			Market = SNew(SNWMarket).Character(Me).Title(S->GetStringField(TEXT("title"))).bShot(true);
			Market->ShotTab(Shot->HasField(TEXT("tab")) ? int32(Shot->GetNumberField(TEXT("tab"))) : 0);
			FString Query, Perk;
			if (Shot->TryGetStringField(TEXT("search"), Query))
			{
				Market->ShotSearch(Query);
			}
			if (Shot->TryGetStringField(TEXT("perk"), Perk))
			{
				Market->ShotPerkFilter(FName(Perk));
			}
			if (Shot->HasField(TEXT("select")))
			{
				Market->ShotSelect(int32(Shot->GetNumberField(TEXT("select"))));
			}
		}
		else if (bShop)
		{
			Shop.Open(Me, S->GetStringField(TEXT("shop")), S->GetStringField(TEXT("title")));
			FString Item;
			if (Shot->TryGetStringField(TEXT("item"), Item))
			{
				Shop.Select(FName(Item), Count);
			}
		}
		else if (!bHud)
		{
			Menu.Open(Me, St);
			FString Recipe;
			if (Shot->TryGetStringField(TEXT("recipe"), Recipe))
			{
				Menu.Select(FName(Recipe), Count);
			}
			const TSharedPtr<FJsonObject>* Improve = nullptr;
			if (Shot->TryGetObjectField(TEXT("improve"), Improve))   // "Improve items" tab: {"slot": bag slot, "mat": material}
			{
				Menu.ShowImprove((*Improve)->HasField(TEXT("slot")) ? int32((*Improve)->GetNumberField(TEXT("slot"))) : INDEX_NONE,
					(*Improve)->HasField(TEXT("mat")) ? uint8((*Improve)->GetNumberField(TEXT("mat"))) : 0);
			}
		}

		auto Frame = [&]()
		{
			TSharedPtr<SWidget> SlateScreen = Market.IsValid() ? StaticCastSharedPtr<SWidget>(Market) : StaticCastSharedPtr<SWidget>(CityMenu);
			if (SlateScreen.IsValid())
			{
				// Two passes: the first measures the layout (texts, lists), the second draws with it.
				// One renderer alive until the end of the process: the cleanup of its draw buffer runs in a task after
				// DrawWidget, and destroying it earlier crashes the process.
				static FWidgetRenderer* Renderer = new FWidgetRenderer(false, false);   // the render target is already sRGB
				Renderer->DrawWidget(RT, SlateScreen.ToSharedRef(), FVector2D(W, H), 0.f);
				FlushRenderingCommands();
				Renderer->DrawWidget(RT, SlateScreen.ToSharedRef(), FVector2D(W, H), 0.f);
				FlushRenderingCommands();
				return;
			}
			FCanvas Canvas(Res, nullptr, FGameTime::CreateUndilated(Menu.FixedNow, 0.0), World->GetFeatureLevel());
			Canvas.Clear(RT->ClearColor);
			UCanvas* C = NewObject<UCanvas>();
			C->Init(W, H, nullptr, &Canvas);
			C->Update();
			if (bHud)
			{
				Hud->DrawShot(C, Me, HudYaw, bHudCombat, HudMap);
			}
			else
			{
				bInventory ? Inventory.Draw(C, Mouse) : bShop ? Shop.Draw(C, Mouse) : Menu.Draw(C, Mouse);
			}
			Canvas.Flush_GameThread(true);
			FlushRenderingCommands();
		};
		// The first frame loads the textures and measures the rows; the second is the one that counts.
		Frame();
		SettleTextures();
		Frame();
		if (!bShop && !bInventory && !bHud && Shot->HasField(TEXT("craft")) && Shot->GetBoolField(TEXT("craft")))
		{
			Menu.ClickCraft();
			Frame();   // the gear item arrives here (t0 of the roll); t counts from it
			Menu.FixedNow += Shot->HasField(TEXT("t")) ? Shot->GetNumberField(TEXT("t")) : 1.0;
			Frame();
			SettleTextures();
			Frame();
		}

		FString DumpFilter;
		if (FParse::Value(*Params, TEXT("dump="), DumpFilter))
		{
			bShop ? Shop.Dump(DumpFilter) : Menu.Dump(DumpFilter);
		}
		TArray<FColor> Pixels;
		Res->ReadPixels(Pixels);
		for (FColor& P : Pixels)
		{
			P.A = 255;
		}
		TArray64<uint8> Png;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
		const FString Out = FPaths::ConvertRelativePathToFull(OutDir / FString::Printf(TEXT("%s_%s.png"), *Base, *Name));
		if (FFileHelper::SaveArrayToFile(Png, *Out))
		{
			UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot: %s"), *Out);
			++Saved;
		}
	}
	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	UE_LOG(LogNWUIShot, Display, TEXT("NWUIShot: %d capturas"), Saved);
	return Saved > 0 ? 0 : 1;
}
