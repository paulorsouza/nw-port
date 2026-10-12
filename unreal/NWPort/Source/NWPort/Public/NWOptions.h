#pragma once

#include "CoreMinimal.h"

class UWorld;

/**
 * Configurações do jogador (tela de configurações do jogo, `lyshineui/options`, docs/hud-original.md), salvas em
 * GameUserSettings.ini [NW.Options]. Só no cliente.
 */
struct NWPORT_API FNWOptions
{
	// Vídeo
	int32 Quality = 2;          // @ui_graphics_settings: 0 Baixa, 1 Média, 2 Alta, 3 Muito Alta (escalabilidade)
	int32 WindowMode = 1;       // @ui_window_mode: 0 tela cheia, 1 em janela, 2 janela sem bordas
	int32 MaxFps = 0;           // @ui_max_fps: 0 = ilimitado
	bool bVSync = false;        // @ui_enable_vsync
	float FovVertical = 50.f;   // @ui_options_fov: graus verticais, como o jogo (câmera padrão 50)
	bool bMotionBlur = false;   // @ui_enable_motionBlur
	bool bShowFps = false;      // @ui_show_fps
	// Áudio (0..1)
	float MasterVolume = 1.f;   // @ui_mastervolume
	float MusicVolume = 0.6f;   // @ui_musicvolume
	float SfxVolume = 1.f;      // @ui_sfxvolume
	float AmbienceVolume = 0.8f;// @ui_ambVolume
	// Jogo
	float SensitivityX = 1.f;   // @ui_looksensitivityhorizontal
	float SensitivityY = 1.f;   // @ui_looksensitivityvertical
	bool bInvertY = false;      // @ui_invertlook

	static FNWOptions& Get();
	void Load();
	void Save() const;
	/** Aplica vídeo e volume geral (os outros valores são lidos por quem usa: câmera, sons, mouse). */
	void Apply(UWorld* World) const;

private:
	bool bLoaded = false;
};
