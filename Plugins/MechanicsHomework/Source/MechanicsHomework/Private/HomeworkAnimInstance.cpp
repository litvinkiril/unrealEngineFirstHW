#include "HomeworkAnimInstance.h"
#include "HomeworkCharacter.h"

void UHomeworkAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    // This callback runs on the game thread; no actor access from a worker-thread update.
    const AHomeworkCharacter* Character = Cast<AHomeworkCharacter>(TryGetPawnOwner());
    bIsSprinting = Character && Character->bIsSprinting;
}
