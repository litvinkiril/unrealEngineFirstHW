#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "HomeworkCharacter.h"
#include "HomeworkAnimInstance.h"
#include "HomeworkTestObserver.h"
#include "InputKeyEventArgs.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHomeworkGameplayTest, "Homework.Gameplay.EndToEnd",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** Exercise physical keys through the template controller, mappings, C++ bindings and saved BPs. */
class FHomeworkGameplayCommand : public IAutomationLatentCommand
{
public:
    explicit FHomeworkGameplayCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual ~FHomeworkGameplayCommand()
    {
        if (Observer.IsValid()) Observer->RemoveFromRoot();
    }
    virtual bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        if (!World || !World->GetFirstPlayerController() || !World->GetFirstPlayerController()->GetPawn())
        {
            if (FPlatformTime::Seconds() - Started > 90.0)
            {
                Test->AddError(TEXT("PIE did not spawn a player within 90 seconds."));
                return true;
            }
            return false;
        }
        if (FPlatformTime::Seconds() - Started > 150.0)
        {
            Test->AddError(TEXT("Gameplay verification timed out."));
            return true;
        }
        auto* PC = World->GetFirstPlayerController();
        auto* Character = Cast<AHomeworkCharacter>(PC->GetPawn());
        if (!Character) { Test->AddError(TEXT("Default pawn is not HomeworkCharacter.")); return true; }
        if (World->GetTimeSeconds() < NextTime) return false;
        auto* Movement = Character->GetCharacterMovement();
        auto* Anim = Cast<UHomeworkAnimInstance>(Character->GetMesh()->GetAnimInstance());
        auto Key = [PC](FKey InKey, bool Down)
        {
            PC->InputKey(FInputKeyEventArgs(nullptr, INPUTDEVICEID_NONE, InKey,
                Down ? IE_Pressed : IE_Released, Down ? 1.f : 0.f, false, FPlatformTime::Cycles64()));
        };
        auto Wait = [this, World](float Seconds) { NextTime = World->GetTimeSeconds() + Seconds; ++Stage; };
        auto State = [Anim](FName Machine)
        {
            return Anim ? Anim->GetCurrentStateName(Anim->GetStateMachineIndex(Machine)) : NAME_None;
        };
        auto Screenshot = [](const TCHAR* Name)
        {
            if (FParse::Param(FCommandLine::Get(), TEXT("HomeworkScreenshots")))
                FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / Name, true, false);
        };
        switch (Stage)
        {
        case 0:
            Wait(1.5f); // Let possession, mapping rebuild and landing finish.
            break;
        case 1:
            Screenshot(TEXT("01-ready.png"));
            Test->TestNotNull(TEXT("Template camera preserved"), Character->FindComponentByClass<UCameraComponent>());
            Test->TestNotNull(TEXT("Homework animation instance assigned"), Anim);
            Test->TestTrue(TEXT("Blueprint is bound to the C++ interaction delegate"),
                Character->OnInteraction.Contains(Character, FName(TEXT("HandleInteraction"))));
            Observer = NewObject<UHomeworkTestObserver>();
            Observer->AddToRoot();
            Character->OnInteraction.AddDynamic(Observer.Get(), &UHomeworkTestObserver::OnInteraction);
            WalkSpeed = Movement->MaxWalkSpeed;
            InitialPosition = Character->GetActorLocation();
            Key(EKeys::W, true);
            Wait(0.35f);
            break;
        case 2:
            Test->TestTrue(TEXT("W moves the template character"), FVector::Dist2D(InitialPosition, Character->GetActorLocation()) > 20.f);
            Test->TestFalse(TEXT("Ordinary movement does not sprint"), Character->bIsSprinting);
            Key(EKeys::LeftShift, true);
            Wait(0.4f);
            break;
        case 3:
            Screenshot(TEXT("02-sprint.png"));
            Test->TestTrue(TEXT("Shift starts sprint through Enhanced Input"), Character->bIsSprinting);
            Test->TestEqual(TEXT("Sprint speed"), Movement->MaxWalkSpeed, Character->SprintSpeed);
            Test->TestTrue(TEXT("Stamina drains during sprint"), Character->Stamina < Character->MaxStamina);
            Test->TestEqual(TEXT("Saved animation enters Sprint"), State(TEXT("Locomotion")), FName(TEXT("Sprint")));
            Key(EKeys::SpaceBar, true);
            Wait(0.2f);
            break;
        case 4:
            Test->TestTrue(TEXT("Space jumps while sprinting"), Movement->IsFalling());
            Test->TestTrue(TEXT("Outer animation leaves ground locomotion"), State(TEXT("Main States")) != FName(TEXT("Locomotion")));
            Key(EKeys::SpaceBar, false);
            Key(EKeys::W, false);
            Key(EKeys::LeftShift, false);
            Wait(2.5f); // Include the template's landing animation before inspecting the cached ground state.
            break;
        case 5:
            Test->AddInfo(FString::Printf(TEXT("After landing: main=%s locomotion=%s actorSprint=%d animSprint=%d"),
                *State(TEXT("Main States")).ToString(), *State(TEXT("Locomotion")).ToString(), Character->bIsSprinting, Anim && Anim->bIsSprinting));
            Test->TestFalse(TEXT("Shift release stops sprint"), Character->bIsSprinting);
            Test->TestEqual(TEXT("Original speed restored"), Movement->MaxWalkSpeed, WalkSpeed);
            Test->TestFalse(TEXT("Character lands after jump"), Movement->IsFalling());
            Test->TestTrue(TEXT("Animation leaves Sprint"), State(TEXT("Locomotion")) != FName(TEXT("Sprint")));
            Key(EKeys::E, true);
            Wait(0.4f);
            break;
        case 6:
            Screenshot(TEXT("03-interaction.png"));
            Test->TestEqual(TEXT("Holding E broadcasts exactly once"), Observer->InteractionCount, 1);
            Key(EKeys::E, false);
            Wait(0.2f);
            break;
        case 7:
            Key(EKeys::E, true);
            Key(EKeys::LeftShift, true);
            Wait(0.25f);
            break;
        case 8:
            Test->TestEqual(TEXT("Second E press broadcasts again"), Observer->InteractionCount, 2);
            Key(EKeys::E, false);
            Test->TestTrue(TEXT("Second Shift press starts sprint"), Character->bIsSprinting);
            Wait(4.2f);
            break;
        case 9:
            Test->TestFalse(TEXT("Exhaustion stops sprint while Shift stays held"), Character->bIsSprinting);
            Test->TestEqual(TEXT("Exhaustion restores walking speed"), Movement->MaxWalkSpeed, WalkSpeed);
            ExhaustedValue = Character->Stamina;
            Wait(1.0f);
            break;
        case 10:
            Test->TestFalse(TEXT("Recovery does not restart held sprint"), Character->bIsSprinting);
            Test->TestTrue(TEXT("Stamina recovers"), Character->Stamina > ExhaustedValue);
            Key(EKeys::LeftShift, false);
            Wait(0.2f);
            break;
        case 11:
            Key(EKeys::LeftShift, true);
            Wait(0.2f);
            break;
        case 12:
            Test->TestTrue(TEXT("Release and repress unlock sprint after recovery"), Character->bIsSprinting);
            Key(EKeys::LeftShift, false);
            Test->AddInfo(TEXT("Verified template movement/camera/jump, C++ sprint, real animation states, Blueprint delegate binding and exhaustion/recovery using physical keys."));
            return true;
        }
        return false;
    }
private:
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    float NextTime = 0;
    float WalkSpeed = 0;
    float ExhaustedValue = 0;
    FVector InitialPosition;
    int32 Stage = 0;
    TWeakObjectPtr<UHomeworkTestObserver> Observer;
};

bool FHomeworkGameplayTest::RunTest(const FString& Parameters)
{
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FHomeworkGameplayCommand>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
