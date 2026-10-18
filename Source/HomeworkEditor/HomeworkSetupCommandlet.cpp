#include "HomeworkSetupCommandlet.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HomeworkCharacter.h"
#include "HomeworkAnimInstance.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "EdGraphSchema_K2.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_TransitionResult.h"
#include "Animation/AnimSequence.h"

namespace HomeworkSetup
{
template<class T> T* AddNode(UEdGraph* Graph, int32 X, int32 Y)
{
    T* Node = NewObject<T>(Graph);
    Graph->AddNode(Node, false, false);
    Node->CreateNewGuid();
    Node->NodePosX = X;
    Node->NodePosY = Y;
    return Node;
}

void FinishNode(UEdGraphNode* Node)
{
    Node->PostPlacedNewNode();
    Node->AllocateDefaultPins();
}

void Connect(UEdGraphPin* From, UEdGraphPin* To)
{
    check(From && To);
    checkf(From->GetOwningNode()->GetSchema()->TryCreateConnection(From, To), TEXT("Cannot connect %s to %s"), *From->PinName.ToString(), *To->PinName.ToString());
}

bool Save(UObject* Asset)
{
    UPackage* Package = Asset->GetOutermost();
    Package->MarkPackageDirty();
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    Args.SaveFlags = SAVE_NoError;
    return UPackage::SavePackage(Package, Asset,
        *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
}

bool Compile(UBlueprint* BP)
{
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FCompilerResultsLog Results;
    FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::None, &Results);
    return Results.NumErrors == 0 && BP->Status != BS_Error;
}

UInputAction* Action(const TCHAR* Name)
{
    const FString Path = FString(TEXT("/Game/Input/Actions/")) + Name;
    UInputAction* Asset = LoadObject<UInputAction>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    if (!Asset)
    {
        Asset = NewObject<UInputAction>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
        FAssetRegistryModule::AssetCreated(Asset);
    }
    Asset->ValueType = EInputActionValueType::Boolean;
    check(Save(Asset));
    return Asset;
}

UK2Node_CallFunction* Call(UEdGraph* Graph, UClass* Class, FName Function, int32 X, int32 Y)
{
    auto* Node = AddNode<UK2Node_CallFunction>(Graph, X, Y);
    Node->SetFromFunction(Class->FindFunctionByName(Function));
    FinishNode(Node);
    return Node;
}

UK2Node_VariableGet* GetBool(UEdGraph* Graph, FName Name, int32 X, int32 Y)
{
    auto* Node = AddNode<UK2Node_VariableGet>(Graph, X, Y);
    Node->VariableReference.SetSelfMember(Name);
    FinishNode(Node);
    return Node;
}

bool Character(UInputAction* Sprint, UInputAction* Interact)
{
    auto* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    check(BP);
    BP->ParentClass = AHomeworkCharacter::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(BP);
    if (!Compile(BP)) return false;

    UEdGraph* Graph = BP->UbergraphPages[0];
    bool AlreadyBound = false;
    for (UEdGraphNode* Node : Graph->Nodes)
        if (auto* Bind = Cast<UK2Node_AddDelegate>(Node))
            AlreadyBound |= Bind->GetPropertyName() == GET_MEMBER_NAME_CHECKED(AHomeworkCharacter, OnInteraction);
    if (!AlreadyBound)
    {
        auto* Begin = AddNode<UK2Node_Event>(Graph, 0, 2200);
        Begin->EventReference.SetExternalMember(TEXT("ReceiveBeginPlay"), AActor::StaticClass());
        Begin->bOverrideFunction = true;
        FinishNode(Begin);

        auto* Bind = AddNode<UK2Node_AddDelegate>(Graph, 300, 2200);
        Bind->SetFromProperty(FindFProperty<FMulticastDelegateProperty>(AHomeworkCharacter::StaticClass(), GET_MEMBER_NAME_CHECKED(AHomeworkCharacter, OnInteraction)), true, AHomeworkCharacter::StaticClass());
        FinishNode(Bind);
        Connect(Begin->FindPinChecked(UEdGraphSchema_K2::PN_Then), Bind->FindPinChecked(UEdGraphSchema_K2::PN_Execute));

        auto* Event = AddNode<UK2Node_CustomEvent>(Graph, 0, 2500);
        Event->CustomFunctionName = TEXT("HandleInteraction");
        FinishNode(Event);
        Connect(Event->FindPinChecked(TEXT("OutputDelegate")), Bind->GetDelegatePin());

        auto* Print = Call(Graph, UKismetSystemLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString), 350, 2500);
        Print->FindPinChecked(TEXT("InString"))->DefaultValue = TEXT("Interaction triggered!");
        Print->FindPinChecked(TEXT("bPrintToScreen"))->DefaultValue = TEXT("true");
        Print->FindPinChecked(TEXT("bPrintToLog"))->DefaultValue = TEXT("true");
        Connect(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then), Print->FindPinChecked(UEdGraphSchema_K2::PN_Execute));
    }
    if (!Compile(BP)) return false;
    auto* CDO = CastChecked<AHomeworkCharacter>(BP->GeneratedClass->GetDefaultObject());
    CDO->SprintAction = Sprint;
    CDO->InteractionAction = Interact;
    return Save(BP);
}

void Transition(UAnimStateNode* From, UAnimStateNode* To, bool EnterSprint)
{
    auto* Node = AddNode<UAnimStateTransitionNode>(From->GetGraph(), (From->NodePosX + To->NodePosX) / 2, EnterSprint ? 50 : 170);
    FinishNode(Node);
    Node->CreateConnections(From, To);
    Node->CrossfadeDuration = 0.15f;
    UEdGraph* Rule = Node->BoundGraph;
    auto* Sprint = GetBool(Rule, TEXT("bIsSprinting"), -650, 0);
    auto* Move = GetBool(Rule, TEXT("ShouldMove"), -650, 140);
    auto* Both = Call(Rule, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, BooleanAND), -400, 0);
    Connect(Sprint->GetValuePin(), Both->FindPinChecked(TEXT("A")));
    Connect(Move->GetValuePin(), Both->FindPinChecked(TEXT("B")));
    UEdGraphPin* Result = Both->GetReturnValuePin();
    if (!EnterSprint)
    {
        auto* Not = Call(Rule, UKismetMathLibrary::StaticClass(), GET_FUNCTION_NAME_CHECKED(UKismetMathLibrary, Not_PreBool), -200, 0);
        Connect(Result, Not->FindPinChecked(TEXT("A")));
        Result = Not->GetReturnValuePin();
    }
    for (UEdGraphNode* RuleNode : Rule->Nodes)
        if (auto* Output = Cast<UAnimGraphNode_TransitionResult>(RuleNode))
            Connect(Result, Output->FindPinChecked(TEXT("bCanEnterTransition")));
}

bool Animation()
{
    auto* BP = LoadObject<UAnimBlueprint>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    check(BP);
    BP->ParentClass = UHomeworkAnimInstance::StaticClass();
    FBlueprintEditorUtils::RefreshAllNodes(BP);
    if (!Compile(BP)) return false;
    TArray<UEdGraph*> Graphs;
    BP->GetAllGraphs(Graphs);
    UAnimStateNode* Walk = nullptr;
    UAnimStateNode* Sprint = nullptr;
    for (auto* Graph : Graphs)
        for (UEdGraphNode* Node : Graph->Nodes)
            if (auto* State = Cast<UAnimStateNode>(Node))
            {
                if (State->GetStateName() == TEXT("Walk / Run")) Walk = State;
                if (State->GetStateName() == TEXT("Sprint")) Sprint = State;
            }
    check(Walk);
    if (!Sprint)
    {
        Sprint = AddNode<UAnimStateNode>(Walk->GetGraph(), Walk->NodePosX + 400, Walk->NodePosY);
        FinishNode(Sprint);
        FBlueprintEditorUtils::RenameGraph(Sprint->BoundGraph, TEXT("Sprint"));
        auto* Player = AddNode<UAnimGraphNode_SequencePlayer>(Sprint->BoundGraph, -300, 0);
        auto* Run = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Jog/MF_Unarmed_Jog_Fwd"));
        check(Run);
        Player->Node.SetSequence(Run);
        Player->Node.SetLoopAnimation(true);
        Player->Node.SetPlayRate(1.5f);
        FinishNode(Player);
        Connect(Player->FindPinChecked(TEXT("Pose")), Sprint->GetPoseSinkPinInsideState());
        Transition(Walk, Sprint, true);
        Transition(Sprint, Walk, false);
    }
    return Compile(BP) && Save(BP);
}

bool Configure()
{
    auto* Sprint = Action(TEXT("IA_Sprint"));
    auto* Interact = Action(TEXT("IA_Interact"));
    auto* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/Input/IMC_Default"));
    check(Context);
    Context->UnmapAllKeysFromAction(Sprint);
    Context->UnmapAllKeysFromAction(Interact);
    Context->MapKey(Sprint, EKeys::LeftShift);
    Context->MapKey(Interact, EKeys::E);
    return Save(Context) && Character(Sprint, Interact) && Animation();
}
}

UHomeworkSetupCommandlet::UHomeworkSetupCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UHomeworkSetupCommandlet::Main(const FString& Params)
{
    if (Params.Contains(TEXT("-Configure")) && !HomeworkSetup::Configure()) return 1;
    FString Report;
    const TCHAR* Paths[] = {
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"),
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"),
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")
    };
    for (auto Path : Paths)
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, Path);
        if (!BP) { UE_LOG(LogTemp, Error, TEXT("Missing %s"), Path); return 1; }
        Report += FString::Printf(TEXT("BLUEPRINT %s PARENT %s\n"), Path, *GetNameSafe(BP->ParentClass));
        TArray<UEdGraph*> Graphs;
        BP->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            Report += FString::Printf(TEXT(" GRAPH %s %s\n"), *Graph->GetName(), *Graph->GetClass()->GetName());
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                Report += FString::Printf(TEXT("  NODE %s | %s\n"), *Node->GetClass()->GetName(), *Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
                for (auto Pin : Node->Pins)
                    Report += FString::Printf(TEXT("    %s %s links=%d default=%s\n"), Pin->Direction == EGPD_Input ? TEXT("IN") : TEXT("OUT"), *Pin->PinName.ToString(), Pin->LinkedTo.Num(), *Pin->DefaultValue);
            }
        }
    }
    FFileHelper::SaveStringToFile(Report, *(FPaths::ProjectSavedDir() / TEXT("AssetInspection.txt")));
    UE_LOG(LogTemp, Display, TEXT("Asset inspection saved."));
    return 0;
}
