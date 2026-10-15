#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "NWUIShotCommandlet.generated.h"

/**
 * Draws a port screen to a PNG without a map, network or PIE (area 4): empty world, a character with the bag and the
 * levels from the scenario file, the same game screen code into a render target.
 *
 *     tools/linux/ui_shot.sh tools/ui_shots/refining.json      ->  extract/ui/shots/refining_<shot>.png
 *
 * Scenario (JSON): "screen" (refining), "lang", "station" {type, tier, name}, "coins", "trade" {TradeSkill: level},
 * "items" [[id, qty]], "shots" [{name, w, h, recipe, count, mouse [x, y], craft, t}]. craft = clicks CONFECCIONAR (CRAFT)
 * and draws t seconds later (roll and end).
 */
UCLASS()
class UNWUIShotCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UNWUIShotCommandlet();
	virtual int32 Main(const FString& Params) override;
};
