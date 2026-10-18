#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StaminaModel.h"
#include "HomeworkCharacter.generated.h"

class UInputAction;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FHomeworkInteractionSignature);

/** Reparent the Blueprint Third Person character to this class.
 * Its existing movement, jumping, camera and mesh remain in the template Blueprint.
 */
UCLASS(Blueprintable)
class MECHANICSHOMEWORK_API AHomeworkCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    AHomeworkCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void UnPossessed() override;

    UPROPERTY(BlueprintAssignable, Category = "Homework|Interaction")
    FHomeworkInteractionSignature OnInteraction;

    UPROPERTY(BlueprintReadOnly, Category = "Homework|Sprint")
    bool bIsSprinting = false;

    UPROPERTY(BlueprintReadOnly, Category = "Homework|Stamina")
    float Stamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Input")
    TObjectPtr<UInputAction> InteractionAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Sprint", meta = (ClampMin = "1.0"))
    float SprintSpeed = 900.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Stamina", meta = (ClampMin = "1.0"))
    float MaxStamina = 100.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Stamina", meta = (ClampMin = "0.0"))
    float StaminaDrainPerSecond = 25.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Stamina", meta = (ClampMin = "0.0"))
    float StaminaRecoveryPerSecond = 20.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Homework|Stamina")
    bool bShowStamina = true;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void StartSprint();
    void StopSprint();
    void Interact();
    void SynchronizeSprintState();
    void DrawStamina() const;

    float WalkSpeedBeforeSprint = 500.0f;
    FHomeworkStaminaModel StaminaModel;
};
