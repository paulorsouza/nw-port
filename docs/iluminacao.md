# Iluminação (esteira 5)

Regras: `docs/esteiras.md`. Testes: `docs/testes/iluminacao.md` (criar no primeiro item). Mapas: `L_QuedaEterna`
primeiro (interiores: igreja, taverna), depois `L_SouthWindsward`.

Objetivo: luz igual à do jogo (ciclo do dia, céu, sol, exposição, luzes locais, interiores) com custo aceitável numa
placa de 8 GB, escolhendo por item entre converter o dado do jogo e usar o recurso equivalente da Unreal.

## O que o port tem hoje (2026-10-13)

- Por mapa: sol (`DirectionalLight`, 8 lux, fixo), luz do céu (captura em tempo real), atmosfera e neblina
  (`tools/ue/import_city.py`). Lumen ligado (GI e reflexos), sombras virtuais, Nanite, distance fields
  (`unreal/NWPort/Config/DefaultEngine.ini`).
- Exposição automática desligada no projeto (`r.DefaultFeature.AutoExposure=False`): a Unreal trava a luminância em 1
  (`SceneView.cpp`). Em `L_QuedaEterna` há um volume `ExposicaoDoJogo` (`tools/ue/city_exposure.py`, etapa
  `sistemas-04`) que deixa só o escuro clarear até 10x (faixa 0,01–0,1, viés −2,32). A rua ficou como antes. Revisar
  aqui.
- Nenhuma luz do jogo: a conversão (`tools/linux/convert_city.sh`) só traz malhas.
- Aviso na tela em Queda Eterna (print do dono 2026-10-13): "o atlas de cache de superfícies do Lumen recebeu um excesso
  de 3–10% de inscrições; considere aumentar `r.LumenScene.SurfaceCache.AtlasSize`". A cidade tem ~5.000 componentes de
  malha e o cache padrão não cabe: partes da cena ficam sem iluminação indireta atualizada (manchas, GI piscando). Opções
  a medir: aumentar o atlas (custa VRAM), reduzir cartões por malha (`r.LumenScene.SurfaceCache.CardMinSize`/malhas
  pequenas fora do Lumen), mesclar props pequenos, ou Lumen em ray tracing por hardware. Mensagem só aparece em
  editor/desenvolvimento, mas o problema existe no jogo final.

## Tentativa desfeita (esteira 4, 2026-10-13)

O que foi feito e por que não serviu, para não repetir:
- 744 luzes da cidade como `PointLight`/`SpotLight` (área virou spot), falloff sem inverso do quadrado, intensidade =
  `DiffuseMultiplier` x escala única, distância de desenho = alcance x fator.
- Resultado no print do dono: lugares claros demais, a luz acende quando o jogador chega perto (pop pela distância de
  desenho), igreja e taverna continuaram erradas; com distância grande o editor travou (GPU 95%, 6,5 GB de VRAM).
- Causas: escala única sem fonte (o jogo não usa unidade física para luz local), falloff do CryEngine diferente do da
  Unreal, distância de desenho inventada, luzes de área aproximadas por spot, sem as regras de interior do jogo
  (`AffectsThisAreaOnly`, vis areas), sem GI equivalente.
- Removido do mapa e do pipeline (`sistemas-05`). Ficou o dado: `extract/cities/luzes/settlement_northwindsward_complete.lights.json`.

## Dados do jogo já levantados

- **Ciclo do dia**: `libs/timeofday/zones/everfall_a.xml` (zona de Queda Eterna; extraído em `extract/_raw/names/libs/timeofday/`).
  Curvas por hora do dia (chave `fração:valor:flags`). De dia: "Sun intensity" 100.000 lux, "Sun color multiplier" 10,
  "EV Min" 5, "EV Max" 17, "EV Auto compensation" 1,5, "Min exposure" 1, "Max exposure" 10. Há também neblina, céu,
  bloom e curva de filme (Film curve).
- **Luzes locais**: componente `LightComponent` → `LightConfiguration` nos slices (struct em
  `vendor/nw-buddy/tools/nwbt/rtti/nwt/generated.go`). Campos: `LightType` (0 pontual, 1 área, 2 projetor, 3 sonda),
  `Color`, `DiffuseMultiplier`, `SpecMultiplier`, `PointMaxDistance`, `PointAttenuationBulbSize`, `AreaWidth/Height`,
  `AreaMaxDistance`, `AreaFOV`, `ProjectorDistance/FOV/Texture`, `CastShadowsSpec` (0xFFFFFFFF = nunca),
  `ViewDistanceMultiplier`, `ViewDistanceCap`, `AnimIndex/AnimSpeed/AnimPhase` (cintilação), `AffectsThisAreaOnly`,
  `IndoorOnly`, `IgnoreVisAreas`, `TodInfluence`, `VolumetricFog(Only)`, `VoxelGIMode`, `Ambient`.
- **Cidade de Queda Eterna**: 744 luzes visíveis (466 pontuais, 73 de área, 205 projetores; 5 com sombra). Igreja
  (`struc_church_t5`): 66 (36/16/14), velas com `DiffuseMultiplier` 0,15–0,3 e alcance 2 m, área 0,1. Taverna
  (`ef_settlement_inn`): 49, lareira (projetor até 20). Postes: `settler_footprinta_northwindsward_setdress_msq2_lights`.
- **nwbt**: `models slices --lights` exporta as luzes (KHR_lights_punctual). Patch do nw-port (não commitado em
  `vendor/nw-buddy`, falta atualizar `patches/nwbt-nwport.patch`): `sliceLight` em
  `commands/models/collect_capitals.go` grava tipo, alcance certo e todos os parâmetros acima nos extras do nó
  `nwlight_N`.

## Perguntas para a RE (skill `nw-re`)

1. Unidade e fórmula da luz local: como `DiffuseMultiplier` e a cor viram radiância no shader do jogo (escala HDR,
   relação com o sol de 100.000 lux e com a exposição).
2. Atenuação: curva do raio (`PointMaxDistance`) e do `AttenuationBulbSize`; luz de área (retângulo, `AreaFOV`).
3. Distância de vista e corte das luzes: fórmula com `ViewDistanceMultiplier` e o cvar equivalente
   (`e_ViewDistRatioLights`?), e se há fade.
4. Interiores: `AffectsThisAreaOnly`/`IndoorOnly` e vis areas — a luz do sol/céu é cortada dentro dos prédios?
5. GI do jogo: há GI dinâmico (`VoxelGIMode`, SVOGI) ou sondas (`LightType` 3, cubemaps) nos interiores?
6. Exposição: como "EV Min/Max", "EV Auto compensation" e "Min/Max exposure" do ciclo do dia entram no cálculo.
7. Cintilação: tabela do `AnimIndex` (curva por índice) e `AnimSpeed`/`AnimPhase`.

## Unreal x conversão (avaliar no levantamento)

| Tema | Converter do jogo | Recurso da Unreal |
|---|---|---|
| Sol, céu, ciclo do dia | curvas do `everfall_a.xml` | Sky Atmosphere, Sky Light em tempo real, unidades físicas (lux/EV100) |
| Exposição | EV Min/Max do jogo | Auto exposure em EV100, Local Exposure |
| GI e interiores | sondas/vis areas do jogo | Lumen (software ou HW ray tracing), emissivo nas velas e janelas |
| Muitas luzes locais | posição, cor, alcance do jogo | MegaLights (5.5+), luz sem sombra, culling por distância com fade |
| Sombras | `CastShadowsSpec` | Virtual Shadow Maps, sombra de contato |
| Cintilação | tabela do `AnimIndex` | Light Function ou componente de animação |
| Neblina e volume | curvas de neblina do ciclo do dia | Exponential Height Fog, Volumetric Fog |

Critério: medir cada escolha (print do jogo original x port no mesmo ponto, `stat gpu`, VRAM) antes de aplicar em
toda a cidade.

## Ordem sugerida

1. Levantamento e RE (perguntas acima); prints do jogo original em pontos fixos (praça, igreja, taverna, rua à noite).
2. Sol, céu, ciclo do dia e exposição em unidades físicas, tirados do `everfall_a.xml`.
3. Luzes locais com a fórmula da RE, num prédio só (igreja), até bater com o print.
4. Interiores (GI, corte de luz externa) e cintilação.
5. Desempenho (MegaLights, culling, escalabilidade) com medição.
6. Cidade inteira e depois Guardaventos.
