# Rotação das armas nos sockets

No jogo a arma entra na junta (`right_hand_attach`, `left_hand_attach`) sem rotação. O importador de glTF do
Blender reorienta os ossos: medido nos dois esqueletos (jogador e Lost), o osso do Blender fica girado -90° no X
local em relação à junta do glb. A exportação Blender -> FBX -> Unreal não muda mais nada (arma presa com rotação
zero no Blender equivale a rotação zero no socket do Unreal; conferido no mesmo quadro de animação, erro 0).

Resultado: `SetRelativeRotation(FRotator(0, 0, -90))` nas três armas (espada e escudo do jogador, espada do Lost).

Como medir de novo: comparar `bone.matrix_local` do Blender com a matriz de bind da junta no glb (convertida
para eixos do Blender); a diferença é a rotação que a arma precisa em relação ao osso. Verificação visual:
renderizar um quadro de `A_rswrd_1h_hvy_atk_01` com e sem a correção (a estocada tem que sair reta).

Os valores antigos (29.6, -2.7, -27.3 / -16.7, -18.9, 131.6) partiam de "no Blender a arma com rotação zero
fica certa", o que não vale.
