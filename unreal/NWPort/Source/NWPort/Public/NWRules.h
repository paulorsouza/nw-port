#pragma once

#include "CoreMinimal.h"

/**
 * Fatos da engenharia reversa do jogo (docs/analise-re/valores.json → tools/make_rules.py → Data/regras.json).
 * O id é o mesmo do valores.json; tools/re_check.py confere quais o port já usa.
 */
struct NWPORT_API FNWRules
{
	/** Valor do fato; Default (com aviso no log, uma vez) se o id não estiver em Data/regras.json. */
	static double Num(const TCHAR* Id, double Default);
};
