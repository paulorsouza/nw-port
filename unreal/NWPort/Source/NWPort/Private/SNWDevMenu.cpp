#include "SNWDevMenu.h"
#include "NWHUD.h"
#include "NWText.h"

#include "Engine/Font.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NWCharacter.h"
#include "NWCombatComponent.h"
#include "NWUILayout.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

static const FLinearColor DevTan(FColor(0xEB, 0xE3, 0xD6));
static const FLinearColor DevGold(FColor(0xED, 0xC6, 0x3E));

static FSlateFontInfo DevFont(int32 Style, int32 Size)
{
	return FSlateFontInfo(FNWUILayout::GameFont(Style), Size);
}

TSharedRef<SWidget> SNWDevMenu::Header(const FString& Label)
{
	return SNew(STextBlock).Font(DevFont(2, 15)).ColorAndOpacity(DevGold).Text(FText::FromString(Label));
}

TSharedRef<SWidget> SNWDevMenu::Button(const FString& Label, TFunction<FText()> State, TFunction<void()> Action)
{
	return SNew(SButton)
		.ButtonColorAndOpacity(FLinearColor(0.12f, 0.12f, 0.13f, 1.f))
		.OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
		.ContentPadding(FMargin(10.f, 6.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f)
			[
				SNew(STextBlock).Font(DevFont(0, 14)).ColorAndOpacity(DevTan).Text(FText::FromString(Label))
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(STextBlock).Font(DevFont(1, 14)).ColorAndOpacity(DevGold).Text_Lambda([State]() { return State ? State() : FText::GetEmpty(); })
			]
		];
}

void SNWDevMenu::Construct(const FArguments& Args)
{
	Owner = Args._Owner;
	TWeakObjectPtr<ANWPlayerController> PC = Owner;
	auto Dev = [PC](ENWDevCommand Cmd) { return [PC, Cmd]() { if (PC.IsValid()) PC->ServerDev(Cmd); }; };
	auto Flag = [PC](bool UNWCombatComponent::* Member)
	{
		return [PC, Member]()
		{
			const ANWCharacter* C = PC.IsValid() ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
			return FText::FromString(C && (C->Combat->*Member) ? NWText::Get(TEXT("nw.dev.on")) : NWText::Get(TEXT("nw.dev.off")));
		};
	};
	auto CVar = [](const TCHAR* Name)
	{
		return [Name]()
		{
			IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
			return FText::FromString(V && V->GetInt() ? NWText::Get(TEXT("nw.dev.on")) : NWText::Get(TEXT("nw.dev.off")));
		};
	};
	auto ToggleCVar = [PC](const TCHAR* Name)
	{
		return [PC, Name]()
		{
			IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name);
			if (V && PC.IsValid())
			{
				PC->LocalConsole(FString::Printf(TEXT("%s %d"), Name, V->GetInt() ? 0 : 1));
			}
		};
	};
	auto SlowMo = [PC]()
	{
		const UWorld* W = PC.IsValid() ? PC->GetWorld() : nullptr;
		return FText::FromString(W && UGameplayStatics::GetGlobalTimeDilation(W) < 0.99f ? NWText::Get(TEXT("nw.dev.on")) : NWText::Get(TEXT("nw.dev.off")));
	};

	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
	auto Add = [&List](TSharedRef<SWidget> W, float Top = 3.f) { List->AddSlot().AutoHeight().Padding(0.f, Top, 0.f, 0.f)[W]; };
	Add(Header(NWText::Get(TEXT("nw.dev.character"))), 0.f);
	Add(Button(NWText::Get(TEXT("nw.dev.no_cooldowns")), Flag(&UNWCombatComponent::bNoCooldowns), Dev(ENWDevCommand::ToggleNoCooldowns)));
	Add(Button(NWText::Get(TEXT("nw.dev.god")), Flag(&UNWCombatComponent::bGodMode), Dev(ENWDevCommand::ToggleGodMode)));
	Add(Button(NWText::Get(TEXT("nw.dev.infinite_stamina")), Flag(&UNWCombatComponent::bInfiniteStamina), Dev(ENWDevCommand::ToggleInfiniteStamina)));
	Add(Button(NWText::Get(TEXT("nw.dev.heal")), nullptr, Dev(ENWDevCommand::HealSelf)));
	Add(Button(NWText::Get(TEXT("nw.dev.die")), nullptr, Dev(ENWDevCommand::KillSelf)));
	Add(Button(NWText::Get(TEXT("nw.dev.clear_cooldowns")), nullptr, Dev(ENWDevCommand::ClearCooldowns)));
	Add(Button(NWText::Get(TEXT("nw.dev.fast_run")), [PC]() -> FText
	{
		const ANWCharacter* C = PC.IsValid() ? Cast<ANWCharacter>(PC->GetPawn()) : nullptr;
		return FText::FromString(C && C->DevSpeedMultiplier > 1.f ? NWText::Get(TEXT("nw.dev.on")) : NWText::Get(TEXT("nw.dev.off")));
	}, Dev(ENWDevCommand::ToggleFastRun)));
	Add(Button(NWText::Get(TEXT("nw.dev.give_tools")), nullptr, Dev(ENWDevCommand::GiveTools)));
	Add(Button(NWText::Get(TEXT("nw.dev.give_level")), nullptr, Dev(ENWDevCommand::GiveLevel)));
	Add(Button(NWText::Get(TEXT("nw.dev.go_ore")), nullptr, Dev(ENWDevCommand::GoToOre)));
	Add(Header(NWText::Get(TEXT("nw.dev.enemies"))), 12.f);
	Add(Button(NWText::Get(TEXT("nw.dev.spawn_lost")), nullptr, Dev(ENWDevCommand::SpawnLost)));
	Add(Button(NWText::Get(TEXT("nw.dev.spawn_dummy")), nullptr, Dev(ENWDevCommand::SpawnDummy)));
	Add(Button(NWText::Get(TEXT("nw.dev.spawn_blocking")), nullptr, Dev(ENWDevCommand::SpawnBlockingDummy)));
	Add(Button(NWText::Get(TEXT("nw.dev.spawn_attacking")), nullptr, Dev(ENWDevCommand::SpawnAttackingDummy)));
	Add(Button(NWText::Get(TEXT("nw.dev.kill_enemies")), nullptr, Dev(ENWDevCommand::KillEnemies)));
	Add(Button(NWText::Get(TEXT("nw.dev.remove_spawned")), nullptr, Dev(ENWDevCommand::RemoveSpawned)));
	Add(Header(NWText::Get(TEXT("nw.dev.world"))), 12.f);
	Add(Button(NWText::Get(TEXT("nw.dev.slow_motion")), SlowMo, Dev(ENWDevCommand::ToggleSlowMotion)));
	Add(Button(NWText::Get(TEXT("nw.dev.go_arena")), nullptr, Dev(ENWDevCommand::TravelArena)));
	Add(Button(NWText::Get(TEXT("nw.dev.go_city")), nullptr, Dev(ENWDevCommand::TravelCity)));
	Add(Header(NWText::Get(TEXT("nw.dev.debug"))), 12.f);
	Add(Button(NWText::Get(TEXT("nw.dev.show_hits")), CVar(TEXT("nw.DebugHits")), ToggleCVar(TEXT("nw.DebugHits"))));
	Add(Button(NWText::Get(TEXT("nw.dev.show_traversal")), CVar(TEXT("nw.DebugTraversal")), ToggleCVar(TEXT("nw.DebugTraversal"))));
	Add(Button(NWText::Get(TEXT("nw.dev.hud_diagnostics")), CVar(TEXT("nw.HudDebug")), ToggleCVar(TEXT("nw.HudDebug"))));
	// Idioma: troca, recarrega as telas e fecha o menu (reabrir mostra no idioma novo).
	Add(Button(NWText::Get(TEXT("nw.dev.language")), nullptr, [PC]()
	{
		NWText::SetLang(NWText::Lang() == TEXT("pt-br") ? TEXT("en") : TEXT("pt"));
		if (ANWPlayerController* P = PC.Get())
		{
			if (ANWHUD* HUD = Cast<ANWHUD>(P->GetHUD()))
			{
				HUD->ReloadLanguage();
			}
			P->CloseMenus();
		}
	}), 12.f);

	ChildSlot
	.HAlign(HAlign_Left)
	.VAlign(VAlign_Center)
	.Padding(40.f, 0.f)
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.018f, 0.92f))
		.Padding(20.f)
		[
			SNew(SBox).WidthOverride(380.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
				[
					SNew(STextBlock).Font(DevFont(2, 22)).ColorAndOpacity(DevTan).Text(FText::FromString(NWText::Get(TEXT("nw.dev.title"))))
				]
				+ SVerticalBox::Slot().AutoHeight()[SNew(SScrollBox) + SScrollBox::Slot()[List]]
			]
		]
	];
}

FReply SNWDevMenu::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Key)
{
	if (Key.GetKey() == EKeys::F10 || Key.GetKey() == EKeys::Escape)
	{
		if (ANWPlayerController* PC = Owner.Get())
		{
			PC->CloseMenus();
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}
