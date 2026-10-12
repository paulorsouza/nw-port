#include "NWRespawnScreen.h"
#include "NWText.h"

#include "Dom/JsonObject.h"
#include "Engine/Canvas.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NWCombatComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

static const TCHAR* Button = TEXT("RespawnMapButtonAccept");

bool FNWRespawnScreen::Load()
{
	const FString File = NWText::DataPath(TEXT("Data/UI/respawn_layout.json"));
	if (!Layout.Load(File))
	{
		return false;
	}
	Layout.ResolveTexture = [](const FString& P) { return FNWUILayout::GameTexture(P); };
	for (int32 i = 0; i < 3; ++i)
	{
		Layout.Fonts[i] = FNWUILayout::GameFont(i);
	}
	// Camadas de foco do botão e da mensagem: o Lua liga conforme o estado.
	if (FNWUIElement* M = E(TEXT("KilledByMessage"))) M->Visible = false;
	if (FNWUIElement* B = E(FString(Button) + TEXT("/heroBorder"))) B->Visible = false;
	if (FNWUIElement* Bg = E(FString(Button) + TEXT("/buttonBg"))) Bg->Tint = FLinearColor(0.10f, 0.13f, 0.20f);
	return true;
}

FString FNWRespawnScreen::Str(const TCHAR* Key) const
{
	return NWText::Get(Key);
}

void FNWRespawnScreen::Draw(UCanvas* Canvas, const FVector2D& Mouse, const UNWCombatComponent& Combat)
{
	if (!bTried)
	{
		bTried = true;
		bLoaded = Load();
	}
	if (!bLoaded)
	{
		return;
	}
	const float Remaining = Combat.GetRespawnRemaining();
	const bool bReady = Remaining <= 0.f;
	if (FNWUIElement* T = E(TEXT("KilledByTitle"))) { T->TextOverride = Str(TEXT("ui_deathrecap_killedby")); T->Visible = !Combat.KilledBy.IsEmpty(); }
	if (FNWUIElement* N = E(TEXT("KilledByName"))) { N->TextOverride = NWText::Resolve(Combat.KilledBy); N->Visible = !Combat.KilledBy.IsEmpty(); }
	if (FNWUIElement* T = E(TEXT("RespawnTimer")))
	{
		T->TextOverride = bReady ? Str(TEXT("ui_deathrecap_respawn_ready")) : FString::FromInt(FMath::CeilToInt(Remaining));
	}
	const FString Prefix(Button);
	FNWUIElement* Root = E(Prefix);
	const bool bHover = Root && Layout.WasShown(Root) && Layout.RectOf(Root).IsInside(FVector2f(Mouse));
	if (Root) Root->AlphaMul = bReady ? 1.f : 0.45f;
	if (FNWUIElement* T = E(Prefix + TEXT("/buttonText"))) { T->TextOverride = Str(TEXT("ui_respawn")); T->FontSize = 28.f; }
	for (const TCHAR* Layer : {TEXT("/buttonBgFocus"), TEXT("/buttonFocus"), TEXT("/frameFocus"), TEXT("/buttonBottomGlow")})
	{
		if (FNWUIElement* L = E(Prefix + Layer)) L->Visible = bHover && bReady;
	}
	Layout.Draw(Canvas);
}

bool FNWRespawnScreen::Click(const FVector2D& Mouse, const UNWCombatComponent& Combat)
{
	FNWUIElement* Root = bLoaded ? E(Button) : nullptr;
	return Root && Combat.GetRespawnRemaining() <= 0.f && Layout.WasShown(Root) && Layout.RectOf(Root).IsInside(FVector2f(Mouse));
}
