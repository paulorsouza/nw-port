# Partículas do jogo no port: estudo pela nevasca da manopla de gelo

2026-10-15, sub-esteira 1a. Pergunta: dá para trazer os efeitos visuais do jogo (partículas) em vez dos placeholders?
Caso estudado: Tempestade de Gelo (`IceMagic_Spell_IceStorm`). Arquivos extraídos em `extract/fx_study/`.

## Como o jogo monta o efeito

- O feitiço cria a slice `items/weapons/spell_aoe_blizzard`, que tem dois `ParticleComponent`:
  `wfxo_IceGaunt_Tempest.Blizzard_AOE` (biblioteca `libs/particles/wfxo_icegaunt_tempest.xml`) e
  `pex_foe_vfx.aoe_vfx_base_0` (`libs/particles/pex_foe_vfx.xml`, anel genérico de área inimiga). Também tem uma
  timeline (`libs/timeline/weapons/icegauntlet_blizzard_01.timeline`, camada PostFX).
- A biblioteca é XML do CryEngine (`ParticleVersion 33`): um emissor raiz com filhos, cada um com `<Params>` de ~25
  atributos. Curvas pela idade da partícula (`ParticleAge=(t=..,v=..)`), acaso (`Random=`), atlas de textura
  (`TilesX/TilesY/VariantCount`), modo de face (`CameraX`, `Free`, `Decal`, `Shape`), CPU ou GPU.

`Blizzard_AOE` (5 emissores ativos):

| Emissor | Tipo | O que é | Textura / material |
|---|---|---|---|
| raiz | malha (`sphere_edge.cgf`, 6 m) | cúpula girando, 2 por vez, vida 1 s, entra e sai em 2 s | `icegauntlet_blizzard_aoe_mesh_2-optimize` (shader `Meshparticle`, ruído `noise_alpha_01` + emissivo `noise_02`) |
| `2_Decal_Spin` | decalque | círculo de neve girando 360°/s, 4,5 m | `icegauntlet_decal_02` (`snow_r_circle001` + normal de neve) |
| `2_Decal_Static_Edge` | decalque | rachadura de gelo, 5,25 m, vida 5,7 s | `impact_frost_03` (`groundcrack_04` + `ice_001`) |
| `2_frostdecal` | decalque | gelo no chão, 4,8 m, vida 5,7 s | `impact_frost_2_less` (`ice_001`) |
| `3_Sphere_Edge_Flurry_gpu` | sprites GPU | flocos no anel de 9 m, 50 a cada 0,2 s, sobem e giram | `Embers_R_Single002_2x2` (atlas 2×2) |

O que o efeito precisa: 1 malha, 3 decalques, 1 emissor de sprites, 6 texturas, 4 materiais com 3 shaders
(`Meshparticle`, `Particles` como decalque, `Particles` como sprite).

## O que o port tem hoje

- nwbt converte malhas e texturas (`.dds`/`.tif` → png); não lê `libs/particles/*.xml` (é XML, lê direto) nem os
  shaders de efeito.
- `ANWFxActor`: esferas com o material `M_NW_FX` (cor + intensidade). Sem sprites, sem decalques, sem atlas.

## Caminhos

**A. Niagara a partir de modelos.** Montar à mão, uma vez, 3 emissores modelo (sprite, malha, decalque) com
parâmetros de usuário; um script Python cria um sistema por efeito do jogo, copia os modelos e preenche os
parâmetros. Bom: desempenho e luz do Niagara. Ruim: a API Python do Niagara não cria módulos, e as curvas
`ParticleAge` não entram por parâmetro simples (precisariam de textura de curva). Exige trabalho manual no editor, que
é do dono.

**B. Emissor próprio em C++ (`UNWParticleComponent`).** Lê o JSON do XML (gerado por um script nosso) e simula na CPU
o subconjunto que o jogo usa: contagem e contínuo, vida, velocidade e ângulo, gravidade, resistência do ar,
turbulência simples, curvas de idade (tamanho, alpha, cor), atlas, face para a câmera ou livre. Desenha sprites
num `UInstancedStaticMeshComponent` de quads, com dados por instância para cor, alpha e quadro; decalques com
`UDecalComponent`; malhas com `UStaticMeshComponent`. Três materiais mestre (sprite aditivo/translúcido, malha com
ruído rolando, decalque). Tudo por script e código, sem passo manual, mesmo jeito das telas do port. Ruim: menos
desempenho que Niagara GPU (centenas de partículas por efeito estão ok; milhares não), sem luz por partícula.

## Decisão do dono (2026-10-15): Niagara

Prova de API feita (`UNWNiagaraTools::ProbeNiagara`, `NWNiagaraTools.cpp`, chamada pelo Python num commandlet): sem
abrir o editor, o C++ de editor cria um sistema a partir de um emissor modelo do motor (`Fountain`), lista os módulos,
lê e grava os parâmetros constantes (`RapidIterationParameters`: vida, tamanho, cor, velocidade em cone, gravidade,
arrasto, taxa de spawn, raio da forma), troca as chaves das curvas (`UNiagaraDataInterfaceCurve` dos nós de entrada,
por reflexão), troca material e atlas do sprite, compila e salva. Existem na 5.8 os renderizadores de sprite, malha,
decalque e luz e os módulos `ScaleSpriteSize`, `ScaleColor`, `ScaleMeshSize`. Exportados do NiagaraEditor:
`AddEmitterToSystem`, `AddScriptModuleToStack`, `SetModuleIsEnabled`, `GetOrCreateStackFunctionInputOverridePin`,
`SetDataInterfaceValueForFunctionInput`; não exportados: `GetOrderedModuleNodes` e a classe `UNiagaraNodeInput`
(contornados percorrendo os nós do grafo e por reflexão).

Plano: `tools/make_particles.py` (XML → JSON por efeito, com curvas e a lista de texturas/malhas/materiais), materiais
mestre de partícula por script (sprite aditivo, sprite translúcido, malha, decalque), `UNWNiagaraTools::BuildEffect`
(um emissor Niagara por emissor do jogo, a partir do modelo do motor), e a área da Tempestade passa a tocar o sistema.

## Recomendação anterior (antes da decisão)

Caminho B, começando pela nevasca:

1. `tools/make_particles.py`: XML → JSON (emissores, curvas, atlas, materiais) + lista de texturas e malhas para o
   nwbt (`--x-dds png`).
2. Materiais mestre `M_NW_Particle_Sprite`, `M_NW_Particle_Mesh`, `M_NW_Particle_Decal` (por script, como os outros).
3. `UNWParticleComponent` com sprite, decalque e malha; a área da Tempestade passa a tocar `Blizzard_AOE`.
4. Comparar com print do jogo; depois estacas, vento, chuva, torre e tumba (outras bibliotecas `wfxo_icegaunt_*`).

Estimativa: 1 a 2 dias para a nevasca ficar parecida, porque é o primeiro efeito e inclui o emissor, os 3 materiais
e o conversor. Os outros efeitos da manopla, que só reaproveitam isso, saem mais rápido. Fica de fora nesta fase:
`EmitterStrength`, `MotionBlur`, `SoftParticle`, `CameraNonFacingFade`, luz dinâmica (`LightSource`) e a timeline
de PostFX.

Vale também para o resto do port: fogo, fumaça e efeitos das outras armas usam o mesmo formato (`docs/pendencias.md`
lista partículas como fora da conversão). Pela tabela de donos, o código comum é da esteira 1 (`NWFxActor`) e o uso no
cenário fica com a esteira 5.

## Pipeline (2026-10-15)

1. `python3 tools/make_particles.py --extract <biblioteca.xml> <Efeito> [...]`: XML → `extract/fx/<Bib>.<Efeito>.json`
   e, pelo `tools/pesado.sh`, materiais (`.mtl`), texturas (png) e malhas (glb) em `extract/fx/src` e `extract/fx/glb`.
2. Com o editor fechado: `UnrealEditor-Cmd NWPort.uproject -run=pythonscript -script="tools/ue/import_fx.py
   extract/fx/<json> [...]"`. Importa texturas e malhas, cria os mestres (`M_NW_P_Sprite`, `_SpriteAdd`, `_Mesh`,
   `_MeshLit`, `_Decal`) e uma instância por material do jogo, traduz cada emissor numa receita
   (`.recipe.json`) e chama `UNWNiagaraTools::BuildEffect`. Relatório em `.build.txt` (lista as constantes não
   achadas). Não rodar dentro do editor aberto: com dezenas de sistemas o editor trava.
3. `tools/ue/fx_notifies.py` põe os efeitos de mão e corpo nas montagens (rodar de novo depois de reimportar o
   conjunto).

O que o `BuildEffect` precisa saber: as constantes dos módulos dos emissores são compiladas dentro dos scripts do
sistema, então são gravadas nos scripts do emissor e também nos de spawn/update do sistema (sem isso o efeito volta aos
valores do modelo); `Loop Duration` do sistema e dos emissores = 1000 s (o modelo `Fountain` some em 5 s); pacote
existente é carregado inteiro e o sistema antigo renomeado antes de salvar por cima (salvar sobre pacote parcial
derruba o commandlet). Decalque Niagara: tamanho é o total (2× o raio do jogo) e orientação (0; 0,7071; 0; 0,7071)
para projetar para baixo; personagens com `SetReceivesDecals(false)`.

Não abrir o editor com o New World original aberto: os dois juntos estouraram os 8 GB da placa e travaram o driver
(precisou reiniciar).
