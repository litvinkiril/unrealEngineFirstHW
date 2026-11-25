#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HomeworkGuard.h"
#include "HomeworkCharacter.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BTCompositeNode.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/PlayerController.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "UnrealClient.h"
#include "PCGVolume.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeworkAITest, "Homework.AI.PatrolChaseNoise",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Compare a blocked straight line with the complete Recast route around the same prop.
static bool HasDetour(UInstancedStaticMeshComponent* Props, AHomeworkGuard* Guard)
{
    UWorld* World = Guard->GetWorld();
    for (int32 Index = 0; Index < Props->GetInstanceCount(); ++Index)
    {
        FTransform Transform;
        Props->GetInstanceTransform(Index, Transform, true);
        const FVector Center = Transform.GetLocation();
        const FVector Start(Center.X - 270, Center.Y, 15);
        const FVector End(Center.X + 270, Center.Y, 15);
        FHitResult Hit;
        if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility) || Hit.GetComponent() != Props)
            continue;
        auto* Path = UNavigationSystemV1::FindPathToLocationSynchronously(World, Start, End, Guard);
        if (Path && Path->IsValid() && !Path->IsPartial() && Path->PathPoints.Num() > 2
            && Path->GetPathLength() > FVector::Dist(Start, End) + 10)
            return true;
    }
    return false;
}

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
        auto* PC = World->GetFirstPlayerController();
        auto* Player = Cast<AHomeworkCharacter>(PC->GetPawn());
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
        auto Key = [PC](FKey Button, bool Down) {
            PC->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, Button,
                Down ? IE_Pressed : IE_Released, Down ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
        };
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
            bool bHasDetour = false;
            for (TActorIterator<APCGVolume> It(World); It; ++It)
            {
                TArray<UInstancedStaticMeshComponent*> Components;
                It->GetComponents(Components);
                for (auto* C : Components)
                {
                    Count += C->GetInstanceCount();
                    bHasDetour |= HasDetour(C, Guard.Get());
                    Test->TestTrue(TEXT("PCG instances block movement"), C->GetCollisionEnabled() != ECollisionEnabled::NoCollision);
                }
            }
            Test->TestEqual(TEXT("PCG saved the full 8 by 8 grid"), Count, 64);
            Test->TestTrue(TEXT("A complete NavMesh path goes around a solid PCG obstacle"), bHasDetour);
            FarAway(); InitialPosition = Guard->GetActorLocation(); Wait(4.f); break;
        }
        case 1:
        {
            Test->TestTrue(TEXT("Guard actually moves while patrolling"), FVector::Dist2D(InitialPosition, Guard->GetActorLocation()) > 30.f);
            Test->TestNull(TEXT("Distant player is not tracked"), BB->GetValueAsObject(TEXT("TargetActor")));
            // The outer lane stays clear for every seed. Put both actors in plain sight.
            Guard->SetActorLocation(FVector(-2980, -150, 98));
            Player->SetActorLocation(FVector(-3610, 310, 98));
            InitialPosition = Guard->GetActorLocation();
            // Keep the pursuing guard visible beside the player in a third-person capture.
            PC->SetControlRotation(FRotator(-10, (InitialPosition - Player->GetActorLocation()).Rotation().Yaw + 34.f, 0));
            Wait(0.7f); break;
        }
        case 2:
        {
            auto* Animation = Guard->GetMesh()->GetAnimInstance();
            Test->TestTrue(TEXT("The pursuing guard plays its locomotion animation"), Animation &&
                Animation->GetCurrentStateName(Animation->GetStateMachineIndex(TEXT("Locomotion"))) == TEXT("Walk / Run"));
            if (!bCapturedPursuit && FParse::Param(FCommandLine::Get(), TEXT("HomeworkScreenshots")))
            {
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots/04-courtyard.png"), false, false);
                bCapturedPursuit = true;
                NextTime = World->GetTimeSeconds() + 0.12f;
                return false;
            }
            Test->TestEqual(TEXT("Visible player interrupts patrol"), BB->GetValueAsObject(TEXT("TargetActor")), static_cast<UObject*>(Player));
            Test->TestTrue(TEXT("Guard closes the distance to the visible player"),
                FVector::Dist2D(Guard->GetActorLocation(), Player->GetActorLocation()) < FVector::Dist2D(InitialPosition, Player->GetActorLocation()) - 40.f);
            FarAway(); Wait(0.7f); break;
        }
        case 3:
        {
            Test->TestNull(TEXT("Player beyond sight radius is forgotten"), BB->GetValueAsObject(TEXT("TargetActor")));
            Guard->SetActorLocation(FVector(-3150, 0, 98));
            NoisePoint = FVector(-3150, 1500, 98);
            Player->SetActorLocation(NoisePoint);
            PC->SetControlRotation(FRotator(0, 180, 0));
            Key(EKeys::LeftShift, true);
            Wait(0.25f); break;
        }
        case 4:
            Test->TestFalse(TEXT("Holding Shift while standing makes no noise"), BB->GetValueAsBool(TEXT("HasNoise")));
            Key(EKeys::W, true);
            Wait(0.42f); break;
        case 5:
            Key(EKeys::W, false);
            Key(EKeys::LeftShift, false);
            Test->TestTrue(TEXT("Player ran before the guard heard footsteps"), FVector::Dist2D(Player->GetActorLocation(), NoisePoint) > 35.f);
            Test->TestTrue(TEXT("Hearing starts an investigation outside acquisition radius"), BB->GetValueAsBool(TEXT("HasNoise")));
            Test->TestNull(TEXT("Hearing does not set a live target"), BB->GetValueAsObject(TEXT("TargetActor")));
            NoisePoint = BB->GetValueAsVector(TEXT("NoiseLocation"));
            Test->TestTrue(TEXT("Noise is near the actual footstep"), FVector::Dist2D(NoisePoint, Player->GetActorLocation()) < 450.f);
            InitialPosition = Guard->GetActorLocation();
            FarAway(); Wait(1.f); break;
        case 6:
            Test->TestTrue(TEXT("Guard remembers the heard position after the player leaves"),
                BB->GetValueAsVector(TEXT("NoiseLocation")).Equals(NoisePoint));
            Test->TestTrue(TEXT("Guard approaches the footstep location"),
                FVector::Dist2D(Guard->GetActorLocation(), NoisePoint) < FVector::Dist2D(InitialPosition, NoisePoint) - 50.f);
            Wait(6.5f); break;
        case 7:
        {
            Test->TestFalse(TEXT("Investigation ends or expires"), BB->GetValueAsBool(TEXT("HasNoise")));
            Test->TestNull(TEXT("The lost target stays forgotten"), BB->GetValueAsObject(TEXT("TargetActor")));
            const auto* Tree = Cast<UBehaviorTreeComponent>(AI->BrainComponent);
            const UBTNode* Active = Tree ? Tree->GetActiveNode() : nullptr;
            while (Active && Active->GetNodeName() != TEXT("Patrol around original spawn"))
                Active = Active->GetParentNode();
            Test->TestNotNull(TEXT("The patrol branch is executing again"), Active);
            Test->TestTrue(TEXT("The next patrol point is within the original home radius"),
                FVector::Dist2D(BB->GetValueAsVector(TEXT("PatrolLocation")), BB->GetValueAsVector(TEXT("HomeLocation"))) <= Guard->PatrolRadius + 1.f);
            return true;
        }
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    TWeakObjectPtr<AHomeworkGuard> Guard;
    double Started = FPlatformTime::Seconds();
    float NextTime = 2.f;
    int32 Stage = 0;
    bool bCapturedPursuit = false;
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
