#include "NWOptions.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Scalability.h"

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
	Float(TEXT("MasterVolume"), MasterVolume);
	Float(TEXT("MusicVolume"), MusicVolume);
	Float(TEXT("SfxVolume"), SfxVolume);
	Float(TEXT("AmbienceVolume"), AmbienceVolume);
	Float(TEXT("SensitivityX"), SensitivityX);
	Float(TEXT("SensitivityY"), SensitivityY);
	Bool(TEXT("InvertY"), bInvertY);
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
	GConfig->SetFloat(Section, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SfxVolume"), SfxVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("AmbienceVolume"), AmbienceVolume, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SensitivityX"), SensitivityX, GGameUserSettingsIni);
	GConfig->SetFloat(Section, TEXT("SensitivityY"), SensitivityY, GGameUserSettingsIni);
	GConfig->SetBool(Section, TEXT("InvertY"), bInvertY, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void FNWOptions::Apply(UWorld* World) const
{
	// Qualidade de vídeo = preset de escalabilidade da Unreal (sombras, texturas, efeitos, pós...).
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
