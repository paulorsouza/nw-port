#pragma once

#include "CoreMinimal.h"

/**
 * Valores vindos da engenharia reversa do jogo. O padrão de Num é o próprio valor; Data/regras.json (opcional,
 * gerado por tools/make_rules.py do cache local re-local/) só sobrescreve.
 */
struct NWPORT_API FNWRules
{
	/** Valor do fato; Default (com aviso no log, uma vez) se o id não estiver em Data/regras.json. */
	static double Num(const TCHAR* Id, double Default);
};
