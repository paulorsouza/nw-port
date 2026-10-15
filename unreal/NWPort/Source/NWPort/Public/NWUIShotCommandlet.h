#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NWUIShotCommandlet.generated.h"

/**
 * Desenha uma tela do port num PNG sem mapa, sem rede e sem PIE (esteira 4): mundo vazio, um personagem com a bolsa e os
 * níveis do arquivo de cenário, o mesmo código de tela do jogo num render target.
 *
 *     tools/linux/ui_shot.sh tools/ui_shots/refining.json      ->  extract/ui/shots/refining_<captura>.png
 *
 * Cenário (JSON): "screen" (refining), "lang", "station" {type, tier, name}, "coins", "trade" {Ofício: nível},
 * "items" [[id, qtd]], "shots" [{name, w, h, recipe, count, mouse [x, y], craft, t}]. craft = clica em CONFECCIONAR e
 * desenha t segundos depois (roll e fim).
 */
UCLASS()
class UNWUIShotCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNWUIShotCommandlet();
	virtual int32 Main(const FString& Params) override;
};
