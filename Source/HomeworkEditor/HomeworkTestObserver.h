#pragma once
#include "CoreMinimal.h"
#include "HomeworkTestObserver.generated.h"

UCLASS()
class UHomeworkTestObserver : public UObject
{
    GENERATED_BODY()
public:
    int32 InteractionCount = 0;
    UFUNCTION() void OnInteraction() { ++InteractionCount; }
};
