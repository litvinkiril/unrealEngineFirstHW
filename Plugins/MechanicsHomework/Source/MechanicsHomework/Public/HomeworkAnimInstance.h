#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "HomeworkAnimInstance.generated.h"

/** Optional parent for the template's root AnimBP (ABP_Manny, not its child ABP_Quinn).
 * Keep the template's existing Event Graph and locomotion graph.
 */
UCLASS(Blueprintable, Transient)
class MECHANICSHOMEWORK_API UHomeworkAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UPROPERTY(BlueprintReadOnly, Transient, Category = "Homework|Animation")
    bool bIsSprinting = false;
};
