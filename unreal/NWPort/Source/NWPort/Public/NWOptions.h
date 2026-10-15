#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UWorld;

/** Action with a rebindable key (Shortcuts tab). Label and default key from the game (libs/config/defaultprofile.xml). */
struct FNWKeyAction
{
	FName Id;
	const TCHAR* LabelKey;
	FKey Default;
};

/**
 * Player settings (the game's settings screen, `lyshineui/options`, docs/game/ui/original.md), saved in
 * GameUserSettings.ini [NW.Options]. Client only.
 */
struct NWPORT_API FNWOptions
{
	// Video
	int32 Quality = 2;          // @ui_graphics_settings: 0 Low, 1 Medium, 2 High, 3 Very High (scalability)
	int32 WindowMode = 1;       // @ui_window_mode: 0 fullscreen, 1 windowed, 2 borderless window
	int32 MaxFps = 0;           // @ui_max_fps: 0 = unlimited
	bool bVSync = false;        // @ui_enable_vsync
	float FovVertical = 50.f;   // @ui_options_fov: vertical degrees, like the game (default camera 50)
	bool bMotionBlur = false;   // @ui_enable_motionBlur
	bool bShowFps = false;      // @ui_show_fps
	float Brightness = 0.5f;    // @ui_options_brightness: 0..1, 0.5 = default curve
	// Audio (0..1)
	float MasterVolume = 1.f;   // @ui_mastervolume
	float MusicVolume = 0.6f;   // @ui_musicvolume
	float SfxVolume = 1.f;      // @ui_sfxvolume
	float AmbienceVolume = 0.8f;// @ui_ambVolume
	// Game
	float SensitivityX = 1.f;   // @ui_looksensitivityhorizontal
	float SensitivityY = 1.f;   // @ui_looksensitivityvertical
	bool bInvertY = false;      // @ui_invertlook
	int32 DamageNumbers = 0;    // @ui_options_damage_numbers_display_mode: 0 floating, 1 stacked
	// Shortcuts (@ui_options_kbm_controls): only the changed keys; the rest stays at the default.
	TMap<FName, FKey> Keys;

	static const TArray<FNWKeyAction>& KeyActions();
	FKey KeyFor(FName Id) const;
	/** Sets the key on the action; if another action already used it, the two swap keys. */
	void SetKey(FName Id, const FKey& Key);
	void ResetKeys() { Keys.Reset(); }

	static FNWOptions& Get();
	void Load();
	void Save() const;
	/** Applies video and master volume (the other values are read by whoever uses them: camera, sounds, mouse). */
	void Apply(UWorld* World) const;

private:
	bool bLoaded = false;
};
