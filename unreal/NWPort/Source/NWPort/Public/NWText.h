#pragma once

#include "CoreMinimal.h"

/**
 * Texts in the clan's two languages (pt-BR and en-US).
 *
 * Language: -nwlang=pt|en, otherwise [NW.Settings] Language in GameUserSettings.ini, otherwise the system's (Portuguese ->
 * pt-br, anything else -> en-us). Table in Data/UI/nw_strings.json (tools/build_strings.py); game screens
 * have one file per language (DataPath).
 *
 * Messages sent by the server travel as "#key|arg0|arg1"; the client translates them with Resolve.
 */
namespace NWText
{
	/** "pt-br" or "en-us". */
	NWPORT_API const FString& Lang();
	/** Changes and saves the language; screens already loaded stay in the previous one until reloaded. */
	NWPORT_API void SetLang(const FString& Code);
	NWPORT_API FString Get(const FString& Key);
	/** Get with {0}, {1}... replaced by the arguments. */
	NWPORT_API FString Format(const FString& Key, const TArray<FString>& Args);
	/** "#key|a|b" -> translated text; any other text comes back unchanged. */
	NWPORT_API FString Resolve(const FString& Message);
	/** Builds "#key|a|b" (server). */
	NWPORT_API FString Key(const FString& Key, const TArray<FString>& Args = {});
	/** "Data/UI/x.json" -> absolute path of "Data/UI/x.<language>.json" if it exists, otherwise of the original. */
	NWPORT_API FString DataPath(const FString& Relative);
}
