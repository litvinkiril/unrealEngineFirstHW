#pragma once
#include "Commandlets/Commandlet.h"
#include "HomeworkSetupCommandlet.generated.h"

/** Reproducible asset setup and inspection; never required to play the saved project. */
UCLASS()
class UHomeworkSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UHomeworkSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};
