#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "Perception/AIPerceptionTypes.h"
#include "HomeworkGuard.generated.h"

class UBehaviorTree;

UCLASS()
class MECHANICSHOMEWORK_API AHomeworkGuard : public ACharacter
{
    GENERATED_BODY()
public:
    AHomeworkGuard();
    UPROPERTY(EditAnywhere, Category="Guard") TObjectPtr<UBehaviorTree> BehaviorTree;
    UPROPERTY(EditAnywhere, Category="Guard", meta=(ClampMin="0")) float PatrolRadius = 1800.f;
    UPROPERTY(EditAnywhere, Category="Guard", meta=(ClampMin="0")) float SightRadius = 1100.f;
    UPROPERTY(EditAnywhere, Category="Guard", meta=(ClampMin="0")) float LoseSightRadius = 1500.f;
};

UCLASS()
class MECHANICSHOMEWORK_API AHomeworkGuardController : public AAIController
{
    GENERATED_BODY()
public:
    AHomeworkGuardController();
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;
    // Called only by the root Behavior Tree service; the controller never issues MoveTo.
    void UpdateAwareness();
    void ClearNoise();
private:
    UFUNCTION() void OnHeard(AActor* Actor, FAIStimulus Stimulus);
    double NoiseExpiresAt = 0.0;
};

/** Updates sight, movement speed and the lifetime of the last heard location. */
UCLASS()
class MECHANICSHOMEWORK_API UBTService_GuardAwareness : public UBTService
{
    GENERATED_BODY()
public:
    UBTService_GuardAwareness();
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

/** Picks a reachable destination relative to the guard's original spawn, not its current position. */
UCLASS()
class MECHANICSHOMEWORK_API UBTTask_GuardPatrolPoint : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_GuardPatrolPoint();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};

UCLASS()
class MECHANICSHOMEWORK_API UBTTask_GuardClearNoise : public UBTTaskNode
{
    GENERATED_BODY()
public:
    UBTTask_GuardClearNoise();
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
};
