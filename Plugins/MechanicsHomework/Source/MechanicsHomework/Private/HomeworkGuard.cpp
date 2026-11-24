#include "HomeworkGuard.h"
#include "HomeworkCharacter.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISense_Hearing.h"
#include "UObject/ConstructorHelpers.h"

AHomeworkGuard::AHomeworkGuard()
{
    AIControllerClass = AHomeworkGuardController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);
    GetCharacterMovement()->MaxWalkSpeed = 230.f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0, 480, 0);
    bUseControllerRotationYaw = false;
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> Mesh(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
    if (Mesh.Succeeded()) GetMesh()->SetSkeletalMeshAsset(Mesh.Object);
    GetMesh()->SetRelativeLocation(FVector(0, 0, -96));
    GetMesh()->SetRelativeRotation(FRotator(0, -90, 0));
    GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

AHomeworkGuardController::AHomeworkGuardController()
{
    auto* Perception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Hearing"));
    SetPerceptionComponent(*Perception);
    auto* Hearing = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
    Hearing->HearingRange = 1800.f;
    Hearing->SetMaxAge(6.f);
    Hearing->DetectionByAffiliation.bDetectEnemies = true;
    Hearing->DetectionByAffiliation.bDetectFriendlies = true;
    Hearing->DetectionByAffiliation.bDetectNeutrals = true;
    Perception->ConfigureSense(*Hearing);
    Perception->OnTargetPerceptionUpdated.AddDynamic(this, &AHomeworkGuardController::OnHeard);
}

void AHomeworkGuardController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    auto* Guard = Cast<AHomeworkGuard>(InPawn);
    if (!Guard || !Guard->BehaviorTree || !Guard->BehaviorTree->BlackboardAsset) return;
    UBlackboardComponent* BB = nullptr;
    if (UseBlackboard(Guard->BehaviorTree->BlackboardAsset, BB))
    {
        BB->SetValueAsVector(TEXT("HomeLocation"), InPawn->GetActorLocation());
        RunBehaviorTree(Guard->BehaviorTree);
    }
}

void AHomeworkGuardController::OnUnPossess()
{
    if (BrainComponent) BrainComponent->StopLogic(TEXT("Unpossessed"));
    Super::OnUnPossess();
}

void AHomeworkGuardController::OnHeard(AActor* Actor, FAIStimulus Stimulus)
{
    auto* Player = Cast<AHomeworkCharacter>(Actor);
    UBlackboardComponent* BB = GetBlackboardComponent();
    if (!BB || !Player || !Player->IsPlayerControlled() || !Stimulus.WasSuccessfullySensed()
        || Stimulus.Type != UAISense::GetSenseID<UAISense_Hearing>()) return;
    // A noise stores a snapshot, not a live target reference: no tracking through walls.
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Location;
    if (Nav && Nav->ProjectPointToNavigation(Stimulus.StimulusLocation, Location, FVector(180,180,250)))
    {
        BB->SetValueAsVector(TEXT("NoiseLocation"), Location.Location);
        BB->SetValueAsBool(TEXT("HasNoise"), true);
        NoiseExpiresAt = GetWorld()->GetTimeSeconds() + 6.0;
    }
}

void AHomeworkGuardController::ClearNoise()
{
    if (auto* BB = GetBlackboardComponent())
    {
        BB->SetValueAsBool(TEXT("HasNoise"), false);
        BB->ClearValue(TEXT("NoiseLocation"));
    }
    NoiseExpiresAt = 0.0;
}

void AHomeworkGuardController::UpdateAwareness()
{
    auto* Guard = Cast<AHomeworkGuard>(GetPawn());
    auto* BB = GetBlackboardComponent();
    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Guard || !BB) return;
    const bool bTracking = BB->GetValueAsObject(TEXT("TargetActor")) != nullptr;
    const float Radius = bTracking ? FMath::Max(Guard->SightRadius, Guard->LoseSightRadius) : Guard->SightRadius;
    const bool bVisible = IsValid(Player) && FVector::DistSquared(Guard->GetActorLocation(), Player->GetActorLocation()) <= FMath::Square(Radius)
        && LineOfSightTo(Player);
    if (bVisible)
    {
        BB->SetValueAsObject(TEXT("TargetActor"), Player);
        ClearNoise();
    }
    else BB->ClearValue(TEXT("TargetActor"));
    if (BB->GetValueAsBool(TEXT("HasNoise")) && GetWorld()->GetTimeSeconds() >= NoiseExpiresAt) ClearNoise();
    Guard->GetCharacterMovement()->MaxWalkSpeed = bVisible ? 650.f : (BB->GetValueAsBool(TEXT("HasNoise")) ? 360.f : 230.f);
}

UBTService_GuardAwareness::UBTService_GuardAwareness()
{
    NodeName = TEXT("Update sight and expire sprint noise");
    Interval = 0.15f;
    RandomDeviation = 0.f;
    bCallTickOnSearchStart = true;
}
void UBTService_GuardAwareness::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    if (auto* AI = Cast<AHomeworkGuardController>(OwnerComp.GetAIOwner())) AI->UpdateAwareness();
}

UBTTask_GuardPatrolPoint::UBTTask_GuardPatrolPoint() { NodeName = TEXT("Choose reachable point around spawn"); }
EBTNodeResult::Type UBTTask_GuardPatrolPoint::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* AI = OwnerComp.GetAIOwner();
    auto* Guard = AI ? Cast<AHomeworkGuard>(AI->GetPawn()) : nullptr;
    auto* BB = OwnerComp.GetBlackboardComponent();
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    if (!Guard || !BB || !Nav) return EBTNodeResult::Failed;
    FNavLocation Point;
    if (!Nav->GetRandomReachablePointInRadius(BB->GetValueAsVector(TEXT("HomeLocation")), Guard->PatrolRadius, Point))
        return EBTNodeResult::Failed;
    BB->SetValueAsVector(TEXT("PatrolLocation"), Point.Location);
    return EBTNodeResult::Succeeded;
}
UBTTask_GuardClearNoise::UBTTask_GuardClearNoise() { NodeName = TEXT("Finish investigation"); }
EBTNodeResult::Type UBTTask_GuardClearNoise::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    auto* AI = Cast<AHomeworkGuardController>(OwnerComp.GetAIOwner());
    if (!AI) return EBTNodeResult::Failed;
    auto* BB = OwnerComp.GetBlackboardComponent();
    if (BB && AI->GetPawn() && FVector::DistSquared2D(AI->GetPawn()->GetActorLocation(),
        BB->GetValueAsVector(TEXT("NoiseLocation"))) <= FMath::Square(250.f))
    {
        AI->ClearNoise();
    }
    // A new footstep during the wait may have moved the investigation destination.
    return EBTNodeResult::Succeeded;
}
