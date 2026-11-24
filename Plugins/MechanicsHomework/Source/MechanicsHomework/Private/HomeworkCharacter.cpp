#include "HomeworkCharacter.h"

#include "EnhancedInputComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputAction.h"
#include "Perception/AISense_Hearing.h"

DEFINE_LOG_CATEGORY_STATIC(LogMechanicsHomework, Log, All);

AHomeworkCharacter::AHomeworkCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AHomeworkCharacter::BeginPlay()
{
    Super::BeginPlay();
    StaminaModel.Configure(MaxStamina, StaminaDrainPerSecond, StaminaRecoveryPerSecond);
    Stamina = StaminaModel.GetValue();
    bIsSprinting = false;
    WalkSpeedBeforeSprint = GetCharacterMovement()->MaxWalkSpeed;
}

void AHomeworkCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!Input)
    {
        UE_LOG(LogMechanicsHomework, Error, TEXT("HomeworkCharacter requires EnhancedInputComponent."));
        return;
    }

    if (ensureMsgf(SprintAction, TEXT("Assign IA_Sprint in BP_ThirdPersonCharacter Class Defaults.")))
    {
        Input->BindAction(SprintAction, ETriggerEvent::Started, this, &AHomeworkCharacter::StartSprint);
        Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &AHomeworkCharacter::StopSprint);
        Input->BindAction(SprintAction, ETriggerEvent::Canceled, this, &AHomeworkCharacter::StopSprint);
    }
    if (ensureMsgf(InteractionAction, TEXT("Assign IA_Interact in BP_ThirdPersonCharacter Class Defaults.")))
    {
        Input->BindAction(InteractionAction, ETriggerEvent::Started, this, &AHomeworkCharacter::Interact);
    }
    // The original Blueprint still installs IMC_Default and handles movement/look/jump.
}

void AHomeworkCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    StaminaModel.Advance(DeltaSeconds);
    SynchronizeSprintState();
    DrawStamina();
    NoiseCooldown -= DeltaSeconds;
    // Footsteps only when actually running on the ground, never while standing/jumping.
    if (HasAuthority() && bIsSprinting && GetVelocity().SizeSquared2D() > FMath::Square(100.f)
        && GetCharacterMovement()->IsMovingOnGround() && NoiseCooldown <= 0.f)
    {
        UAISense_Hearing::ReportNoiseEvent(this, GetActorLocation(), 1.f, this, 1800.f, TEXT("Sprint"));
        NoiseCooldown = 0.4f;
    }
}

void AHomeworkCharacter::StartSprint()
{
    StaminaModel.Press();
    SynchronizeSprintState();
}

void AHomeworkCharacter::StopSprint()
{
    StaminaModel.Release();
    SynchronizeSprintState();
}

void AHomeworkCharacter::SynchronizeSprintState()
{
    const bool bNewSprintState = StaminaModel.IsSprinting();
    if (bNewSprintState != bIsSprinting)
    {
        UCharacterMovementComponent* Movement = GetCharacterMovement();
        if (bNewSprintState)
        {
            WalkSpeedBeforeSprint = Movement->MaxWalkSpeed;
            Movement->MaxWalkSpeed = FMath::Max(SprintSpeed, WalkSpeedBeforeSprint);
        }
        else
        {
            Movement->MaxWalkSpeed = WalkSpeedBeforeSprint;
        }
        bIsSprinting = bNewSprintState;
    }
    Stamina = StaminaModel.GetValue();
}

void AHomeworkCharacter::Interact()
{
    OnInteraction.Broadcast();
}

void AHomeworkCharacter::DrawStamina() const
{
    if (bShowStamina && IsLocallyControlled() && GEngine)
    {
        // A stable per-character key updates one line instead of adding a line every frame.
        GEngine->AddOnScreenDebugMessage(
            static_cast<uint64>(GetUniqueID()), 0.25f,
            bIsSprinting ? FColor::Yellow : FColor::Green,
            FString::Printf(TEXT("Stamina: %.0f / %.0f"), Stamina, StaminaModel.GetMaximum()));
    }
}

void AHomeworkCharacter::UnPossessed()
{
    StopSprint();
    Super::UnPossessed();
}

void AHomeworkCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopSprint();
    if (GEngine)
    {
        GEngine->RemoveOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()));
    }
    Super::EndPlay(EndPlayReason);
}
