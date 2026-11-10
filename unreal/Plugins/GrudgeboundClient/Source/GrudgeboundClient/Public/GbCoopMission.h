#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GbWidgets.h"
#include "GbCoopMission.generated.h"

class UInstancedStaticMeshComponent;
class UTextRenderComponent;
class UCameraComponent;
class USpringArmComponent;

UENUM(BlueprintType)
enum class EGbBellPhase : uint8 { Arrival, Search, Ritual, Revelation, Boss, Decision, Complete, Defeat };
UENUM(BlueprintType)
enum class EGbBellObject : uint8 { Mira, Nara, Silk, Eiden, ClueNotice, ClueContract, ClueSeal, AnchorA, AnchorB, Collector };
UENUM(BlueprintType)
enum class EGbBellEnding : uint8 { None, ReturnNames, BreakBell };

USTRUCT(BlueprintType)
struct FGbBellMission
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") EGbBellPhase Phase = EGbBellPhase::Arrival;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") uint8 Clues = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") float Channel = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") float BossHp = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") float BossMaxHp = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") float OpenUntil = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") float PulseAt = 0;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") EGbBellEnding Ending = EGbBellEnding::None;
	UPROPERTY(BlueprintReadOnly, Category="Grudgebound|Coop") int32 Votes = 0;
};

namespace GbBell
{
	bool Collect(FGbBellMission& Mission, EGbBellObject Object);
	int32 RequiredAnchors(int32 PartySize);
	EGbBellEnding ResolveVotes(const TArray<EGbBellEnding>& Votes);
	FString Objective(const FGbBellMission& Mission);
	FVector Position(EGbBellObject Object);
	FString Label(EGbBellObject Object);
}

UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopState : public AGameStateBase
{
	GENERATED_BODY()
public:
	AGbCoopState();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Grudgebound|Coop") FGbBellMission Mission;
	UPROPERTY(Replicated, BlueprintReadOnly, Category="Grudgebound|Coop") FString Line;
};

/** Static NPCs, clues, seal anchors and Collector proxy; inputs are routed through the owned controller. */
UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopObject : public AActor
{
	GENERATED_BODY()
public:
	AGbCoopObject();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
	UPROPERTY(EditAnywhere, Category="Grudgebound|Coop") EGbBellObject Kind = EGbBellObject::Mira;
private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Body;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Trim;
	UPROPERTY() TObjectPtr<UTextRenderComponent> Nameplate;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopCharacter : public ACharacter
{
	GENERATED_BODY()
public:
	AGbCoopCharacter();
	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* Controller) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	void Hurt(float Damage);
	void Restore(float Amount);
	bool IsDown() const { return Health <= 0; }
	UPROPERTY(ReplicatedUsing=OnRep_Health, BlueprintReadOnly, Category="Grudgebound|Coop") float Health = 100;
	UPROPERTY(ReplicatedUsing=OnRep_Color, BlueprintReadOnly, Category="Grudgebound|Coop") int32 BinderIndex = 0;
private:
	UFUNCTION() void OnRep_Health();
	UFUNCTION() void OnRep_Color();
	UPROPERTY() TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY() TObjectPtr<UCameraComponent> Camera;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Robe;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Face;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API UGbCoopHud : public UGbWidgetBase
{
	GENERATED_BODY()
protected:
	virtual void BuildTree() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
private:
	UPROPERTY() TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY() TObjectPtr<UTextBlock> MissionText;
	UPROPERTY() TObjectPtr<UTextBlock> PartyText;
	UPROPERTY() TObjectPtr<UTextBlock> DialogueText;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopController : public APlayerController
{
	GENERATED_BODY()
public:
	AGbCoopController();
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaSeconds) override;
	UFUNCTION(Server, Reliable) void ServerUse(bool bHeld);
	UFUNCTION(Server, Reliable) void ServerCast(FVector_NetQuantize Target);
	UFUNCTION(Server, Reliable) void ServerVote(EGbBellEnding Ending);
	UFUNCTION(Server, Reliable) void ServerRetry();
	UFUNCTION(Client, Reliable) void ClientLine(const FString& Text);
	FString PersonalLine;
	float LineUntil = 0;
private:
	void UsePressed(); void UseReleased(); void Cast(); void VoteNames(); void VoteBell(); void Retry();
	void BotTick(float DeltaSeconds);
	void SetupLocalUi();
	bool bLocalUiReady = false;
	float LastCast = -10;
	float BotUseTime = 0;
	float NextLocalCast = 0;
	float BotAge = 0;
	float CompleteTime = 0;
	int32 LastObservedPhase = -1;
	int32 LastObservedClues = -1;
	int32 LastObservedParty = -1;
	bool bBot = false;
	bool bBotManualKeys = false;
	UPROPERTY() TObjectPtr<UGbCoopHud> Hud;
	friend class AGbCoopMode;
};

UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopMode : public AGameModeBase
{
	GENERATED_BODY()
public:
	AGbCoopMode();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& Id, FString& Error) override;
	virtual void PostLogin(APlayerController* Player) override;
	virtual void Logout(AController* Player) override;
	void Use(AGbCoopController* Player, bool bHeld);
	void Cast(AGbCoopController* Player, const FVector& Target);
	void Vote(AGbCoopController* Player, EGbBellEnding Ending);
	void Retry();
private:
	AGbCoopState* State() const;
	void Phase(EGbBellPhase NewPhase, const FString& Line);
	void FinishVote();
	TMap<TWeakObjectPtr<AGbCoopController>, EGbBellObject> Channels;
	TMap<TWeakObjectPtr<AGbCoopController>, TWeakObjectPtr<AGbCoopCharacter>> Revives;
	TMap<TWeakObjectPtr<AGbCoopController>, float> ReviveTime;
	TMap<TWeakObjectPtr<AGbCoopController>, EGbBellEnding> Votes;
	int32 NextBinder = 0;
};

/** Frozen canal district; procedural assets keep the prototype self-contained. */
UCLASS()
class GRUDGEBOUNDCLIENT_API AGbCoopDistrict : public AActor
{
	GENERATED_BODY()
public:
	AGbCoopDistrict();
	virtual void OnConstruction(const FTransform& Transform) override;
private:
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Stone;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Ink;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Paper;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lanterns;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Wood;
};
