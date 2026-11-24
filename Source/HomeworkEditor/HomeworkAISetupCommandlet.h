#pragma once
#include "Commandlets/Commandlet.h"
#include "HomeworkAISetupCommandlet.generated.h"

/** Rebuilds the saved HW2 assets from a small, reviewable setup. */
UCLASS()
class UHomeworkAISetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHomeworkAISetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
