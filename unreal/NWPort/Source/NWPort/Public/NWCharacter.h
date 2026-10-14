#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "NWCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UNWCombatComponent;
class UNWTraversalComponent;
class USpringArmComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UDataTable;
class UAnimInstance;
class UBlendSpace;
struct FNWCharacterRecord;

// Locomoção base trocada por outra esteira (montaria, coleta) enquanto valer: ver SetLocomotionOverride.
USTRUCT()
struct FNWLocomotionOverride
{
	GENERATED_BODY()
	UPROPERTY() TObjectPtr<UBlendSpace> Space;
	UPROPERTY() int32 Priority = 0;
	UPROPERTY() bool bNoSprint = true;
};

// Uma arma do jogador (troca no 1/2): malha, tabela de ações, locomoção e habilidades de Q/R/F.
USTRUCT(BlueprintType)
struct FNWWeaponSet
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) FName Id;                     // "Sword", "Hatchet" (chave do loadout e do abilities.json)
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
	UPROPERTY(EditAnywhere) bool bUsesShield = false;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UDataTable> Actions;
	UPROPERTY(EditAnywhere) TSoftObjectPtr<UBlendSpace> Locomotion;
	UPROPERTY(EditAnywhere) FString HudIcon;              // ícone "drawing" do jogo
	UPROPERTY(EditAnywhere) TArray<FName> Abilities;
	// Osso da mão que segura (o arco vai na esquerda).
	UPROPERTY(EditAnywhere) FName Socket = TEXT("right_hand_attach");
	// Números da arma (javelindata_itemdefinitions_weapons, T2). Dano na escala do port: espada 64 do jogo = 100.
	UPROPERTY(EditAnywhere) float BaseDamage = 100.f;
	UPROPERTY(EditAnywhere) float CritChance = 0.07f;
	UPROPERTY(EditAnywhere) float CritDamageMultiplier = 1.3f;
	UPROPERTY(EditAnywhere) float StaggerDamage = 36.f;
	UPROPERTY(EditAnywhere) float BlockStaminaDamage = 36.f;
	// Estabilidade de quem bloqueia com esta arma (com escudo, a do escudo).
	UPROPERTY(EditAnywhere) float BlockStability = 30.f;
	// Arma de mira (arco): botão direito mira, esquerdo atira.
	UPROPERTY(EditAnywhere) bool bAims = false;
	// Carga do pesado (Attack_Start_Charge): soltar antes de MinHold = leve; FullCharge = sai o carregado sozinho.
	UPROPERTY(EditAnywhere) float HeavyMinHold = 0.333f;
	UPROPERTY(EditAnywhere) float HeavyFullCharge = 0.667f;
};

/**
 * Personagem do jogador (e base do Lost e das criaturas). A implementação é dividida por esteira (docs/esteiras.md):
 * NWCharacter.cpp (movimento, câmera, input, menus), NWCharacterArmas.cpp (armas, combate, habilidades, boneco),
 * NWCharacterSave.cpp (salvar/restaurar). Coleta e montaria ficam nos componentes (Gather, Mount); teclas novas pelo
 * NW_REGISTER_INPUT (NWInputRegistry.h), comandos do F10 pelo NW_REGISTER_DEV (NWDevRegistry.h).
 *
 * Câmera por cima do ombro, strafe com a câmera (como no jogo com a arma Câmera por cima do ombro, strafe com a câmera (como no jogo com a arma
 * em mãos) e sprint orientado ao movimento. Sem controlador, serve de boneco de treino.
 *
 * Controles (padrão do jogo, libs/config/defaultprofile.xml): WASD, mouse, Espaço pula (ou sobe degrau,
 * vault, mantle), Shift esquiva, Alt esquerdo corre, botão esquerdo ataca (segurar = pesado), direito bloqueia,
 * Q/R/F habilidades, 1/2 troca de arma, E coleta, K menu de habilidades, F10 menu de dev. Corre sozinho para a
 * frente (sprint); Ctrl segurado anda.
 */
UCLASS()
class NWPORT_API ANWCharacter : public ACharacter
{
	GENERATED_BODY()
	friend class UNWGatherComponent;

public:
	ANWCharacter(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<USpringArmComponent> CameraBoom;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UCameraComponent> Camera;
	// Câmera mais perto que isto do peito (cm): o próprio jogador deixa de ver o personagem.
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraHideDistance = 75.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Sword;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Shield;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UNWCombatComponent> Combat;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UNWTraversalComponent> Traversal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWInventoryComponent> Inventory;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWGatherComponent> Gather;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWProgressionComponent> Progression;
	// Montaria (esteira 3, NWMountComponent).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWMountComponent> Mount;
	/** Foco e tecla de interagir com NPC, estação, armazém (esteira 4, NWInteractable.h). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWInteractorComponent> Interactor;
	// IA de combate (inimigos). Criado desligado; ligado (UseBrain), substitui o TickDummy.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<class UNWBrainComponent> Brain;
	/** Liga o cérebro com o estilo e os ataques dados (servidor). */
	void UseBrain(uint8 Style, const TArray<struct FNWBrainAttack>& Attacks);
	// Flecha na corda enquanto mira com o arco.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> NockedArrow;
	// Arma guardada no quadril (a que não está em uso).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "NW") TObjectPtr<UStaticMeshComponent> Stowed;

	// Armas do jogador (vazio = personagem de uma arma só, como o Lost).
	UPROPERTY(EditAnywhere, Category = "NW|Armas") TArray<FNWWeaponSet> WeaponSets;
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon, BlueprintReadOnly, Category = "NW|Armas") int32 ActiveWeapon = 0;
	// As duas armas em uso (teclas 1 e 2), índices em WeaponSets; como no jogo, duas por vez.
	UPROPERTY(ReplicatedUsing = OnRep_ActiveWeapon, BlueprintReadOnly, Category = "NW|Armas") TArray<int32> Loadout = {0, 1};
	/** A arma do par que não está na mão (INDEX_NONE com uma arma só). */
	int32 OtherWeapon() const;
	/** Servidor: troca o par de armas (menu de dev) e saca a primeira. */
	void SetLoadout(int32 First, int32 Second);
	// Troca de arma (1/2): toca o sacar da arma nova.
	void SwitchWeapon(int32 Index);
	void SetWeaponAbilities(int32 SetIndex, const TArray<FName>& New);

	/**
	 * Troca a locomoção base do ABP (o pino Locomotion) sem mexer no grafo: montaria, coleta. Vale a de maior
	 * prioridade; sem nenhuma, a da arma em uso. bNoSprint tira a animação de sprint enquanto valer. Local em cada
	 * máquina: quem replica o estado (ex.: montado) chama nos clientes também.
	 */
	void SetLocomotionOverride(FName OverrideOwner, UBlendSpace* Space, int32 Priority = 0, bool bNoSprint = true);
	void ClearLocomotionOverride(FName OverrideOwner);
	bool LocomotionBlocksSprint() const;
	/** Menu aberto (maestria, inventário, dev): o personagem não age. */
	bool InMenu() const;

	// Assets do personagem (o padrão é o jogador de espada e escudo; o Lost troca no construtor).
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<USkeletalMesh> MeshAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftClassPtr<UAnimInstance> AnimClass;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UStaticMesh> SwordAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UStaticMesh> ShieldAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UDataTable> DamageTableAsset;
	UPROPERTY(EditAnywhere, Category = "NW|Assets") TSoftObjectPtr<UDataTable> ActionTableAsset;

	// Velocidades das animações de locomoção do jogo: andar 1,65 m/s, correr (combate) 4 m/s, sprint.
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float WalkSpeed = 165.f;
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float RunSpeed = 400.f;
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float SprintSpeed = 650.f;

	// Câmera do jogo (DynamicCamera): foco 1,7 m acima dos pés e 0,5 m à direita, 2 m atrás; zoom de 0,2 a 6 m
	// (1 m por clique da roda); FOV 50° vertical. Opções do jogador mudam o FOV (NW.Options).
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraFovVertical = 50.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoom = 200.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoomMin = 20.f;
	UPROPERTY(EditAnywhere, Category = "NW|Câmera") float CameraZoomMax = 600.f;
	// Velocidade de giro durante a janela Homing do ataque (graus/s).
	UPROPERTY(EditAnywhere, Category = "NW|Movimento") float HomingTurnRate = 540.f;
	// Habilidades em Q, R e F (nomes de linha da tabela de ações).
	UPROPERTY(EditAnywhere, Category = "NW|Combate") TArray<FName> Abilities = {TEXT("ShieldBash"), TEXT("SpinAttack"), TEXT("SwipeStab")};
	// Troca as habilidades de Q/R/F (menu de habilidades) e grava a escolha para a próxima sessão.
	void SetAbilities(const TArray<FName>& New);
	// Relê as escolhas de Q/R/F do GameUserSettings.ini.
	void LoadAbilityLoadout();

	// Servidor com nw-core: aplica arma, vida e habilidades salvas / copia o estado atual para salvar.
	void RestoreFrom(const FNWCharacterRecord& Record);
	void CaptureTo(FNWCharacterRecord& Record) const;

	// Sprint automático indo para a frente (como no jogo); Ctrl segurado anda.
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") bool bSprinting = false;
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") bool bWalking = false;
	// Menu de dev: multiplica todas as velocidades (correr mais rápido para atravessar o mapa).
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "NW") float DevSpeedMultiplier = 1.f;
	// Menu de dev: voo livre, atravessa o cenário. Frente segue a câmera (com inclinação); Espaço sobe, Ctrl desce.
	UPROPERTY(ReplicatedUsing = OnRep_DevFlying, BlueprintReadOnly, Category = "NW") bool bDevFlying = false;
	void ApplyDevFlying();
	/** Tremida do jogo (javelindata_camerashake, DT_NWCameraShake) na câmera deste personagem, se for local. */
	void PlayCameraShake(FName Id);
	// Tranco curto na câmera (golpe dado/recebido): 1 = ~5 cm por ~0,25 s.
	void AddCameraKick(float Amount) { CameraKick = FMath::Max(CameraKick, Amount); CameraKickTime = 0.f; }

	// Boneco de treino (sem jogador): fica com o escudo levantado.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") bool bDummyBlocks = false;
	// Boneco de treino: ataca o jogador mais próximo dentro do alcance, um combo a cada intervalo.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") bool bDummyAttacks = false;
	/** Jogador simulado do teste de carga (ANWLoadTest): controle de IA, mas spawn de criaturas e IA o tratam como
	 *  jogador. Use IsPlayerLike() onde a regra é "é um jogador?". */
	UPROPERTY(Transient) bool bSimPlayer = false;
	bool IsPlayerLike() const { return bSimPlayer || IsPlayerControlled(); }
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float DummyAttackInterval = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float DummyAttackRange = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") int32 DummyComboHits = 3;
	// Inimigo: persegue o jogador dentro deste raio (0 = fica parado) e escolhe um ataque da lista.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") float AggroRange = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NW|Boneco") TArray<FName> DummyAttackActions;

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	/** Refaz o mapeamento com as teclas das opções (aba Atalhos). */
	void RebuildKeys();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;
	// Assets importados depois de o editor abrir não entram pelo construtor; carrega aqui o que faltar.
	void LoadMissingAssets();

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AttackPressed();
	void AttackReleased();
	void BlockPressed();
	void BlockReleased();
	void Dodge();
	void SkillMenuPressed();
	void DevMenuPressed();
	void UpdateCameraClipHide();
	/** Câmera com os estados do jogo (libs/camera/gamecamera.xml, docs/hud-original.md): normal, sprint, inventário. */
	void UpdateCamera(float DeltaSeconds);
	void Zoom(const FInputActionValue& Value);
	bool bHiddenForCamera = false;
	void JumpPressed();
	void JumpReleased();
	// Pulo, travessia ou ação de combate em curso: não aceita outra ação.
	bool CanAct() const;
	void WalkPressed();
	void WalkReleased();
	/** Dono: sprint quando o movimento pedido vai para a frente da câmera e nada impede. */
	void UpdateAutoSprint();
	void Ability(int32 Index);
	void Ability1() { Ability(0); }
	void Ability2() { Ability(1); }
	void Ability3() { Ability(2); }

	UFUNCTION(Server, Reliable) void ServerSetSprinting(bool bNew);
	UFUNCTION(Server, Reliable) void ServerSetWalking(bool bNew);
	UFUNCTION(Server, Reliable) void ServerSwitchWeapon(int32 Index);
	UFUNCTION(Server, Reliable) void ServerSetWeaponAbilities(int32 SetIndex, const TArray<FName>& New);
	UFUNCTION() void OnRep_ActiveWeapon();
	UFUNCTION() void OnRep_DevFlying() { ApplyDevFlying(); }
	float CameraKick = 0.f;
	float CameraKickTime = 0.f;
	// Tremida em curso (parâmetros da linha, tempo, alvo atual e anterior, próxima troca de alvo).
	struct FShake
	{
		bool bActive = false;
		FVector Shift = FVector::ZeroVector, Angle = FVector::ZeroVector;
		float FadeIn = 0.f, Sustain = 0.f, FadeOut = 0.f, Period = 0.03f, Randomness = 0.f, Time = 0.f, NextTarget = 0.f;
		float Flip = 1.f;
		FVector FromShift = FVector::ZeroVector, ToShift = FVector::ZeroVector, FromAngle = FVector::ZeroVector, ToAngle = FVector::ZeroVector;
	} ActiveShake;
	UPROPERTY() TObjectPtr<UDataTable> ShakeTable;
	bool bFlyUp = false;
	void ApplyWeapon();
	/** Preenche WeaponSets com as armas do jogador (NWCharacterArmas.cpp). */
	void AddPlayerWeaponSets();
	/** Põe no ABP a locomoção que vale agora (override de maior prioridade ou a da arma). */
	void RefreshLocomotion();
	UPROPERTY() TMap<FName, FNWLocomotionOverride> LocomotionOverrides;
	void Weapon1() { if (Loadout.IsValidIndex(0)) SwitchWeapon(Loadout[0]); }
	void Weapon2() { if (Loadout.IsValidIndex(1)) SwitchWeapon(Loadout[1]); }
	void ApplySprint();

private:
	void BuildInput();
	void MapKeys();
	/** Corrida automática (tecla = do jogo): segue para a frente da câmera até apertar frente/trás. */
	void AutoRunPressed() { bAutoRun = !bAutoRun; }
	bool bAutoRun = false;
	UPROPERTY() TObjectPtr<UInputAction> IA_AutoRun;
	UPROPERTY() TObjectPtr<UInputAction> IA_Map;
	void MapPressed();

	UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
	UPROPERTY() TObjectPtr<UInputAction> IA_Move;
	UPROPERTY() TObjectPtr<UInputAction> IA_Look;
	UPROPERTY() TObjectPtr<UInputAction> IA_Zoom;
	UPROPERTY() TObjectPtr<UInputAction> IA_Attack;
	UPROPERTY() TObjectPtr<UInputAction> IA_Block;
	UPROPERTY() TObjectPtr<UInputAction> IA_Dodge;
	UPROPERTY() TObjectPtr<UInputAction> IA_Walk;
	UPROPERTY() TObjectPtr<UInputAction> IA_Jump;
	UPROPERTY() TObjectPtr<UInputAction> IA_SkillMenu;
	UPROPERTY() TObjectPtr<UInputAction> IA_DevMenu;
	UPROPERTY() TObjectPtr<UInputAction> IA_Weapon1;
	UPROPERTY() TObjectPtr<UInputAction> IA_Weapon2;
	UPROPERTY() TObjectPtr<UInputAction> IA_Potion;
	TObjectPtr<UInputAction> IA_Inventory;
	TObjectPtr<UInputAction> IA_Interact;
	void UsePotion();
	void InventoryPressed();
	void InteractPressed();
	UPROPERTY() TArray<TObjectPtr<UInputAction>> IA_Abilities;
	// Teclas das esteiras (FNWInputRegistry), na mesma ordem do registro.
	UPROPERTY() TArray<TObjectPtr<UInputAction>> IA_Registered;

	void TickDummy(float DeltaSeconds);

	FVector2D LastMoveInput = FVector2D::ZeroVector;
	float DummyTimer = 0.f;
	int32 DummyHitsLeft = 0;
	bool bAttackFromSprint = false;
	// O aperto do ataque não foi clique de menu: só então a soltura vai ao servidor. Soltar sem carga vira golpe
	// leve no servidor (ServerAttackInput); um clique de menu não pode virar ataque na soltura.
	bool bAttackPressAccepted = false;
};
