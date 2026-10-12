#pragma once

#include "CoreMinimal.h"

/**
 * Textos nos dois idiomas do clã (pt-BR e en-US).
 *
 * Idioma: -nwlang=pt|en, senão [NW.Settings] Language no GameUserSettings.ini, senão o do sistema (português ->
 * pt-br, qualquer outro -> en-us). Tabela em Data/UI/nw_strings.json (tools/build_strings.py); telas do jogo
 * têm um arquivo por idioma (DataPath).
 *
 * Mensagens que saem do servidor viajam como "#chave|arg0|arg1"; o cliente traduz com Resolve.
 */
namespace NWText
{
	/** "pt-br" ou "en-us". */
	NWPORT_API const FString& Lang();
	/** Troca e grava o idioma; telas já carregadas continuam no anterior até recarregar. */
	NWPORT_API void SetLang(const FString& Code);
	NWPORT_API FString Get(const FString& Key);
	/** Get com {0}, {1}... trocados pelos argumentos. */
	NWPORT_API FString Format(const FString& Key, const TArray<FString>& Args);
	/** "#chave|a|b" -> texto traduzido; qualquer outro texto volta igual. */
	NWPORT_API FString Resolve(const FString& Message);
	/** Monta "#chave|a|b" (servidor). */
	NWPORT_API FString Key(const FString& Key, const TArray<FString>& Args = {});
	/** "Data/UI/x.json" -> caminho absoluto de "Data/UI/x.<idioma>.json" se existir, senão do original. */
	NWPORT_API FString DataPath(const FString& Relative);
}
