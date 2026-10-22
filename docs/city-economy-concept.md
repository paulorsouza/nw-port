# City Economy Concept

Concept proposal for an New World port to Unreal with player-owned cities.

## Concept

Cities are bought, not conquered. A governor runs the city as a business with a closed treasury. An NPC authority, the Crown, guarantees a minimum price for raw material. Every city cost scales with the region's population. Wars are a separate competitive system with no economic payout.

| Role | Wants | Gets from the city |
| --- | --- | --- |
| Gatherer | a buyer at any volume | the floor price always; a premium when the governor needs material |
| Crafter | stable input prices, better stations | a price corridor, station bonuses, weekly buffs |
| Governor | a city that ranks well | a weekly reward by band; no access to the treasury |
| Company (war) | competition | ranking, banner, cosmetics; nothing economic |

## Design pillars

1. **Predictable base prices.** Raw material has a floor, city consumables have a ceiling, crafted goods float.
2. **The same game at any population.** Per-player rules are per capita; city rules scale with the active players of the region.
3. **The governor manages, never mints.** The treasury only moves coins that players put in.
4. **Wars are sport.** No tax, no land and no treasury at stake.

## Core loop

Gather, sell raw material to the city, the treasury funds projects, better stations attract crafting, crafting tax refills the treasury.

Coin flow:

| From | To | What |
| --- | --- | --- |
| Crown (new coins) | Player | floor price of raw material |
| City treasury | Player | premium above the floor |
| Player | City treasury | list items, crafting tax |
| Player | Trading Post | player-to-player trade |
| City treasury | Destroyed | projects, buffs, weapons, cap excess |

The governor's reward is also new coin from the Crown, paid to the governor, not to the treasury.

- **Positive loop.** Station levels and buffs raise crafting quality, crafting raises tax income, income funds the next level.
- **Negative loops.** The treasury cap forces spending; simulated attacks force defense spending; the dire band forces a sale of the city. Each loop has a visible timer: weekly rollover, attack every 2 days, 24 h auction.
- **Exit of coins.** Projects, buffs, defense, cap excess and the resale fee are destroyed. Nothing else leaves the city economy.

## Price floor

The Crown buys any raw material at the floor price, always, up to a weekly limit per character. The treasury pays only the premium the governor sets above the floor.

- Price paid = floor + premium. The floor is new coin from the Crown. The premium comes from the treasury, from 0 to 4× the floor. An empty treasury means zero premium; the floor stays.
- Floor and ranges are set per material by the admin. Default floor: 0.4× the reference market price.
- Limit per character: 5,000 units per material per week. Bounds the Crown's spend and bot farming.
- Limit per city: each city buys at most a fixed amount of each material per week and sells at most a fixed amount of each list item. When a city reaches its cap, players sell and buy in the next city. This spreads the market across cities instead of concentrating it in one.
- Material goes to the city stock and is never deleted. The stock is consumed by station upgrades; the last levels absorb very large amounts.

With more gatherers than crafters and no floor, the price falls toward zero, gatherers leave, and the price spikes when demand returns. With a floor, nobody lists below what the Crown pays on the spot, so the market price stops at the floor and the Crown absorbs the surplus.

## Scaling with population

Every city cost is multiplied by f(N), so the time to complete a project is the same in a region of 10 and a region of 100,000.

```latex
f(N) = \left(\frac{N}{N_0}\right)^{\alpha}
```

- N: active players of the region in the week, 4-week moving average, minimum 10. N₀: the population the base tables were tuned for. α = 1; values below 1 let large cities progress faster.
- Scales with f(N): initial treasury, station, buff and defense costs, dire thresholds, score normalisation. The governor's reward scales with √(N/N₀).
- Does not scale: per-character limits, price ranges, tax range.

```latex
T_{t+1} = T_t + N(L + I) - A - f(N)\,C_{base}
```

T = treasury, L = list sales per player, I = crafting tax per player, A = premium paid, C_base = project costs at N₀. Income grows with N and costs with f(N); with α = 1 the ratio is constant.

Aggregate model, 16 weeks, active governor. Treasury at week 16 divided by weekly treasury income:

| Active players per region | Fixed costs | Costs scaled by population |
| --- | --- | --- |
| 10 | 0.3 | 4.8 |
| 100 | 14.1 | 4.8 |
| 1,000 | 18.5 | 4.8 |
| 10,000 | 18.9 | 4.8 |
| 100,000 | 18.9 | 4.8 |

Treasury cap: 12 weeks.

## Treasury and governor

- **Income:** sales of the city's list (potions, catalysts) and the crafting tax (0 to 10%). No Trading Post fee.
- **Spend:** premium on raw material, stations, buffs, defense. Everything but the premium is destroyed.
- **Cap:** 12 weeks of income. Excess is destroyed at the weekly rollover.
- **No governor:** tax 0, list revenue destroyed, prices at the middle of the range. An abandoned city does not accumulate.
- **Ownership:** fixed price when unowned; 24 h auction otherwise, 10% resale fee destroyed. The governor cannot withdraw from the treasury.
- **Weekly band:** score (material bought for projects, levels reached, buffs, mean station level) divided by f(N) and compared with the other cities. Dire = below minimum activity (100 × N raw material, 10 × N list items, one buff). Dire forces a 24 h auction from the starting price.
- **Reward:** new coin from the Crown to the governor by band, scaled by √(N/N₀).
- **Late game:** the last station levels cost very large amounts of material and give relevant crafting bonuses. The city with the highest levels becomes the preferred place to craft, which raises its tax income and its material demand. This is the long-term sink for material and the reason to keep buying.
- **Admin tools:** inject or remove treasury coins, set floors, ranges and caps per city and per material. Every change is logged in the city log.

## Wars outside the economy

Wars are cross-region PvP events with a ranking and direct rewards per battle, like every other PvP mode in the game. No city is at stake: wars never give tax, prices, treasury access, station bonuses or ownership. Removing the payout removes the incentive for cartels and keeps the competitive part.

## Tuning knobs

| Knob | Default | Effect when raised |
| --- | --- | --- |
| Floor ratio | 0.4× market price | safer gatherers, more Crown coin |
| Premium range | 0 to 4× the floor | stronger governor lever on material inflow |
| City weekly cap (buy and sell) | about one third of the region's weekly supply per material | fewer cities needed to absorb the market |
| Character weekly limit | 5,000 per material | more Crown coin, more bot value |
| α | 1 | large cities progress slower relative to small ones |
| Treasury cap | 12 weeks of income | less spending pressure |
| Late-level material cost | grows steeply per level | longer race for the top crafting city |
| Late-level bonus | relevant (quality, GS, yield) | stronger pull of crafters to the top city |
| Crafting tax range | 0 to 10% | more treasury income, less crafting in the city |
| Treasury injection (admin) | 0 | live-ops lever for a stalled city |
