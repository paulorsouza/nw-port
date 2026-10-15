#include "NWOptions.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"
#include "NWInputRegistry.h"

static const TCHAR* Section = TEXT("NW.Options");

FNWOptions& FNWOptions::Get()
{
	static FNWOptions Options;
	if (!Options.bLoaded)
	{
		Options.Load();
	}
	return Options;
}

const TArray<FNWKeyAction>& FNWOptions::KeyActions()
{
	// Order and keys from the game (defaultprofile.xml: moveforward w, autorun equals, walk lctrl, dodge lshift,
	// ability1-3 q/r/f, quickslot-weapon1/2 1/2, quickslot-consumable-1 3, toggleInventoryWindow tab).
	static TArray<FNWKeyAction> Actions = {
		{TEXT("MoveForward"), TEXT("ui_movefoward"), EKeys::W},
		{TEXT("MoveBack"), TEXT("ui_movebackward"), EKeys::S},
		{TEXT("MoveLeft"), TEXT("ui_moveleft"), EKeys::A},
		{TEXT("MoveRight"), TEXT("ui_moveright"), EKeys::D},
		{TEXT("AutoRun"), TEXT("ui_runtoggle"), EKeys::Equals},
		{TEXT("Walk"), TEXT("ui_walk"), EKeys::LeftControl},
		{TEXT("Jump"), TEXT("ui_mounts_keybind_jump"), EKeys::SpaceBar},
		{TEXT("Dodge"), TEXT("ui_rolldodge"), EKeys::LeftShift},
		{TEXT("Attack"), TEXT("ui_attack"), EKeys::LeftMouseButton},
		{TEXT("Block"), TEXT("ui_block"), EKeys::RightMouseButton},
		{TEXT("Ability1"), TEXT("ui_ability1"), EKeys::Q},
		{TEXT("Ability2"), TEXT("ui_ability2"), EKeys::R},
		{TEXT("Ability3"), TEXT("ui_ability3"), EKeys::F},
		{TEXT("Weapon1"), TEXT("ui_weapon1"), EKeys::One},
		{TEXT("Weapon2"), TEXT("ui_weapon2"), EKeys::Two},
		{TEXT("Consumable1"), TEXT("ui_consumable1"), EKeys::Three},
		{TEXT("Consumable2"), TEXT("ui_consumable2"), EKeys::Four},
		{TEXT("Consumable3"), TEXT("ui_consumable3"), EKeys::Five},
		{TEXT("Consumable4"), TEXT("ui_consumable4"), EKeys::Six},
		{TEXT("Interact"), TEXT("ui_interact"), EKeys::E},
		{TEXT("Inventory"), TEXT("ui_inventory"), EKeys::Tab},
		{TEXT("Map"), TEXT("ui_map"), EKeys::M},
		{TEXT("Mastery"), TEXT("ui_mastery"), EKeys::K},
	};
	// Area keys (NW_REGISTER_INPUT), after the game's; built on the first call, already at runtime.
	static bool bRegistered = false;
	if (!bRegistered)
	{
		bRegistered = true;
		for (const FNWInputBinding& B : FNWInputRegistry::All())
		{
			Actions.Add({B.Id, B.LabelKey, B.Default});
		}
	}
	return Actions;
}

FKey FNWOptions::KeyFor(FName Id) const
{
	if (const FKey* K = Keys.Find(Id))
	{
		return *K;
	}
	const FNWKeyAction* A = KeyActions().FindByPredicate([Id](const FNWKeyAction& X) { return X.Id == Id; });
	return A ? A->Default : EKeys::Invalid;
}

void FNWOptions::SetKey(FName Id, const FKey& Key)
{
	const FKey Old = KeyFor(Id);
	for (const FNWKeyAction& A : KeyActions())
	{
		if (A.Id != Id && KeyFor(A.Id) == Key)
		{
			Keys.Add(A.Id, Old);
		}
	}
	Keys.Add(Id, Key);
	// A key equal to the default does not need to be saved.
	for (const FNWKeyAction& A : KeyActions())
	{
		if (const FKey* K = Keys.Find(A.Id); K && *K == A.Default)
		{
			Keys.Remove(A.Id);
		}
	}
}

void FNWOptions::Load()
{
	bLoaded = true;
	auto Int = [](const TCHAR* Key, int32& V) { GConfig->GetInt(Section, Key, V, GGameUserSettingsIni); };
	auto Float = [](const TCHAR* Key, float& V) { GConfig->GetFloat(Section, Key, V, GGameUserSettingsIni); };
	auto Bool = [](const TCHAR* Key, bool& V) { GConfig->GetBool(Section, Key, V, GGameUserSettingsIni); };
	Int(TEXT("Quality"), Quality);
	Int(TEXT("WindowMode"), WindowMode);
	Int(TEXT("MaxFps"), MaxFps);
	Bool(TEXT("VSync"), bVSync);
	Float(TEXT("FovVertical"), FovVertical);
	Bool(TEXT("MotionBlur"), bMotionBlur);
	Bool(TEXT("ShowFps"), bShowFps);
	Float(TEXT("Brightness"), Brightness);
	Float(TEXT("MasterVolume"), MasterVolume);
	Float(TEXT("MusicVolume"), MusicVolume);
	Float(TEXT("SfxVolume"), SfxVolume);
	Float(TEXT("AmbienceVolume"), AmbienceVolume);
	Float(TEXT("SensitivityX"), SensitivityX);
	Float(TEXT("SensitivityY"), SensitivityY);
	Bool(TEXT("InvertY"), bInvertY);
	Int(TEXT("DamageNumbers"), DamageNumbers);
	for (const FNWKeyAction& A : KeyActions())
	{
		FString Name;
		if (GConfig->GetString(Section, *(TEXT("Key.") + A.Id.ToString()), Name, GGameUserSettingsIni) && FKey(*Name).IsValid())
		{
			Keys.Add(A.Id, FKey(*Name));
		}
	}
	Quality = FMath::Clamp(Quality, 0, 3);
	FovVertical = FMath::Clamp(FovVertical, 40.f, 80.f);
}

void FNWOptions::Save() const
{
	GConfig->SetInt(Section, TEXT("Quality"), Quality, GGameUserSettingsIni);
	GConfig->SetInt(Section, TEXT("WindowMode"), WindowMode, GGameUserSettingsIni);
	GConfig->SetInt(Section, TEXT("MaxFps"), MaxFps, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("VSync"), bVSync, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("FovVertical"), FovVertical, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("MotionBlur"), bMotionBlur, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("ShowFps"), bShowFps, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("Brightness"), Brightness, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SfxVolume"), SfxVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("AmbienceVolume"), AmbienceVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SensitivityX"), SensitivityX, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SensitivityY"), SensitivityY, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->SetInt(Section, TEXT("DamageNumbers"), DamageNumbers, GGameUserSettingsIni);
	for (const FNWKeyAction& A : KeyActions())
	{
		const FString Key = TEXT("Key.") + A.Id.ToString();
		if (const FKey* K = Keys.Find(A.Id))
		{
			GConfig->SetString(Section, *Key, *K->GetFName().ToString(), GGameUserSettingsIni);
		}
		else
		{
			GConfig->RemoveKey(Section, *Key, GGameUserSettingsIni);
		}
	}
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FNWOptions::Apply(UWorld* World) const
{
	// Video quality = Unreal scalability preset (shadows, textures, effects, post...).
	Scalability::FQualityLevels Levels = Scalability::GetQualityLevels();
	Levels.SetFromSingleQualityLevel(Quality);
	Scalability::SetQualityLevels(Levels);
	auto SetCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			V->Set(Value, ECVF_SetByGameSetting);
		}
	};
	SetCVar(TEXT("t.MaxFPS"), MaxFps);
	SetCVar(TEXT("r.VSync"), bVSync ? 1 : 0);
	// Brightness: tonemapper gamma from 1.8 to 2.8; in the middle (0.5) it is 0 = Unreal's default sRGB curve.
	if (IConsoleVariable* Gamma = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TonemapperGamma")))
	{
		Gamma->Set(FMath::IsNearlyEqual(Brightness, 0.5f, 0.01f) ? 0.f : 1.8f + Brightness * 1.f, ECVF_SetByGameSetting);
	}
	SetCVar(TEXT("r.MotionBlurQuality"), bMotionBlur ? 3 : 0);
	if (UGameUserSettings* S = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const EWindowMode::Type Mode = WindowMode == 0 ? EWindowMode::Fullscreen : WindowMode == 2 ? EWindowMode::WindowedFullscreen : EWindowMode::Windowed;
		if (S->GetFullscreenMode() != Mode)
		{
			S->SetFullscreenMode(Mode);
			S->ApplyResolutionSettings(false);
		}
	}
	if (World)
	{
		if (FAudioDeviceHandle Audio = World->GetAudioDevice())
		{
			Audio->SetTransientPrimaryVolume(MasterVolume);
		}
	}
}
