#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HomeworkGuard.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "Perception/AISense_Hearing.h"
#include "PCGVolume.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeworkAITest, "Homework.AI.PatrolChaseNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FHomeworkAICommand : public IAutomationLatentCommand
{
public:
    explicit FHomeworkAICommand(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 90)
        { Test->AddError(TEXT("AI test timed out waiting for PIE/navigation.")); return true; }
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController()) return false;
        APawn* Player = World->GetFirstPlayerController()->GetPawn();
        if (!Player || World->GetTimeSeconds() < NextTime) return false;
        if (!Guard.IsValid())
        {
            TActorIterator<AHomeworkGuard> It(World);
            if (It) Guard = *It;
            if (!Guard.IsValid()) return false;
        }
        auto* AI = Cast<AHomeworkGuardController>(Guard->GetController());
        auto* BB = AI ? AI->GetBlackboardComponent() : nullptr;
        auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (!BB || !Nav || Nav->IsNavigationBuildInProgress()) return false;
        auto Wait = [this, World](float Seconds) { NextTime = World->GetTimeSeconds() + Seconds; ++Stage; };
        auto FarAway = [&]() {
            const FVector G = Guard->GetActorLocation();
            Player->SetActorLocation(FVector(G.X >= 0 ? -3700 : 3700, G.Y >= 0 ? -3700 : 3700, 110), false);
        };
        switch (Stage)
        {
        case 0:
        {
            auto* BT = Cast<UBehaviorTreeComponent>(AI->BrainComponent);
            Test->TestTrue(TEXT("The Behavior Tree is running"), BT && BT->IsRunning());
            int32 Count = 0;
            for (TActorIterator<APCGVolume> It(World); It; ++It)
            {
                TArray<UInstancedStaticMeshComponent*> Components;
                It->GetComponents(Components);
                for (auto* C : Components)
                {
                    Count += C->GetInstanceCount();
                    Test->TestTrue(TEXT("PCG instances block movement"), C->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
                }
            }
            Test->TestTrue(TEXT("PCG produced at least 40 props"), Count >= 40);
            FarAway(); InitialPosition = Guard->GetActorLocation(); Wait(4.f); break;
        }
        case 1:
        {
            Test->TestTrue(TEXT("Guard actually moves while patrolling"), FVector::Dist2D(InitialPosition, Guard->GetActorLocation()) > 30.f);
            Test->TestNull(TEXT("Distant player is not tracked"), BB->GetValueAsObject(TEXT("TargetActor")));
            const FVector G = Guard->GetActorLocation();
            FNavLocation P;
            if (!Nav->ProjectPointToNavigation(G + FVector(160,0,0), P, FVector(180,180,250)))
            { Test->AddError(TEXT("No nearby navigation for sight test")); return true; }
            Player->SetActorLocation(P.Location + FVector(0,0,100), false);
            Wait(0.7f); break;
        }
        case 2:
            Test->TestEqual(TEXT("Visible player interrupts patrol"), BB->GetValueAsObject(TEXT("TargetActor")), static_cast<UObject*>(Player));
            FarAway(); Wait(0.7f); break;
        case 3:
        {
            Test->TestNull(TEXT("Player beyond sight radius is forgotten"), BB->GetValueAsObject(TEXT("TargetActor")));
            const FVector G = Guard->GetActorLocation();
            FNavLocation P;
            const FVector TowardCenter = FVector(-G.X, -G.Y, 0).GetSafeNormal();
            const FVector Direction = TowardCenter.IsNearlyZero() ? FVector(1,0,0) : TowardCenter;
            if (!Nav->ProjectPointToNavigation(G + Direction * 1450.f, P, FVector(150,150,300)))
            { Test->AddError(TEXT("No navigation for hearing test")); return true; }
            NoisePoint = P.Location;
            Player->SetActorLocation(NoisePoint + FVector(0,0,100), false);
            UAISense_Hearing::ReportNoiseEvent(World, NoisePoint, 1.f, Player, 1800.f, TEXT("Sprint"));
            Wait(0.4f); break;
        }
        case 4:
            Test->TestTrue(TEXT("Hearing starts an investigation outside acquisition radius"), BB->GetValueAsBool(TEXT("HasNoise")));
            Test->TestNull(TEXT("Hearing does not set a live target"), BB->GetValueAsObject(TEXT("TargetActor")));
            Test->TestTrue(TEXT("Stored noise location is a snapshot"), BB->GetValueAsVector(TEXT("NoiseLocation")).Equals(NoisePoint, 200.f));
            FarAway(); Wait(7.f); break;
        case 5:
            Test->TestFalse(TEXT("Investigation ends or expires"), BB->GetValueAsBool(TEXT("HasNoise")));
            Test->TestNull(TEXT("AI returns to patrol"), BB->GetValueAsObject(TEXT("TargetActor")));
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHomeworkGuard> Guard;
    double Started = FPlatformTime::Seconds();
    float NextTime = 2.f;
    int32 Stage = 0;
    FVector InitialPosition, NoisePoint;
};

bool FHomeworkAITest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/Homework2/Lvl_PCGPatrol")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHomeworkAICommand>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
