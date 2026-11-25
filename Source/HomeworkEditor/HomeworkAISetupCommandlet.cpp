#include "HomeworkAISetupCommandlet.h"
#include "HomeworkGuard.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/GameModeBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetCompilingManager.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "BehaviorTreeGraph.h"
#include "BehaviorTreeGraphNode.h"
#include "EdGraphSchema_BehaviorTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/BrushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/WorldSettings.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGSubsystem.h"
#include "PCGVolume.h"
#include "Elements/PCGCreatePointsGrid.h"
#include "Elements/PCGTransformPoints.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "Containers/Ticker.h"

namespace HomeworkAISetup
{
template<class T> T* Asset(const TCHAR* Name)
{
    FString Path = FString(TEXT("/Game/Homework2/")) + Name;
    auto* Result = NewObject<T>(CreatePackage(*Path), Name, RF_Public | RF_Standalone);
    FAssetRegistryModule::AssetCreated(Result);
    return Result;
}
bool Save(UObject* Object)
{
    FSavePackageArgs Args;
    Args.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(Object->GetOutermost(), Object,
        *FPackageName::LongPackageNameToFilename(Object->GetOutermost()->GetName(), TEXT(".uasset")), Args);
}
void Key(UObject* Node, const TCHAR* Name)
{
    auto* Property = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("BlackboardKey"));
    check(Property);
    Property->ContainerPtrToValuePtr<FBlackboardKeySelector>(Node)->SelectedKeyName = Name;
}
void Byte(UObject* Node, const TCHAR* PropertyName, uint8 Value)
{
    auto* Property = FindFProperty<FByteProperty>(Node->GetClass(), PropertyName);
    check(Property);
    Property->SetPropertyValue_InContainer(Node, Value);
}
void Child(UBTCompositeNode* Parent, UBTNode* Node, UBTDecorator* Condition = nullptr)
{
    FBTCompositeChild Entry;
    Entry.ChildComposite = Cast<UBTCompositeNode>(Node);
    Entry.ChildTask = Cast<UBTTaskNode>(Node);
    if (Condition) Entry.Decorators.Add(Condition);
    Parent->Children.Add(Entry);
}
UBTDecorator* Condition(UBehaviorTree* Tree, const TCHAR* Name)
{
    auto* D = NewObject<UBTDecorator_Blackboard>(Tree);
    Key(D, Name);
    Byte(D, TEXT("BasicOperation"), EBasicKeyOperation::Set);
    Byte(D, TEXT("OperationType"), EBasicKeyOperation::Set);
    Byte(D, TEXT("FlowAbortMode"), EBTFlowAbortMode::Both);
    D->BuildDescription();
    return D;
}
UBTTask_MoveTo* Move(UBehaviorTree* Tree, const TCHAR* Name)
{
    auto* Task = NewObject<UBTTask_MoveTo>(Tree);
    Key(Task, Name);
    Task->AcceptableRadius = 90.f;
    Task->bObserveBlackboardValue = true;
    Task->bAllowPartialPath = false;
    return Task;
}
UBTTask_Wait* Wait(UBehaviorTree* Tree, float Seconds)
{
    auto* Task = NewObject<UBTTask_Wait>(Tree);
    Task->WaitTime = Seconds;
    Task->RandomDeviation = 0.f;
    return Task;
}
UBlackboardData* Blackboard()
{
    if (auto* Existing = LoadObject<UBlackboardData>(nullptr, TEXT("/Game/Homework2/BB_Guard.BB_Guard"), nullptr, LOAD_NoWarn))
        return Existing;
    auto* BB = Asset<UBlackboardData>(TEXT("BB_Guard"));
    for (FName Name : {FName(TEXT("HomeLocation")), FName(TEXT("PatrolLocation")), FName(TEXT("NoiseLocation"))})
    {
        FBlackboardEntry E; E.EntryName = Name;
        E.KeyType = NewObject<UBlackboardKeyType_Vector>(BB); BB->Keys.Add(E);
    }
    FBlackboardEntry Target; Target.EntryName = TEXT("TargetActor");
    auto* ObjectKey = NewObject<UBlackboardKeyType_Object>(BB); ObjectKey->BaseClass = AActor::StaticClass();
    Target.KeyType = ObjectKey; BB->Keys.Add(Target);
    FBlackboardEntry Noise; Noise.EntryName = TEXT("HasNoise");
    Noise.KeyType = NewObject<UBlackboardKeyType_Bool>(BB); BB->Keys.Add(Noise);
    return Save(BB) ? BB : nullptr;
}
UBehaviorTree* Behavior()
{
    if (auto* Existing = LoadObject<UBehaviorTree>(nullptr, TEXT("/Game/Homework2/BT_Guard.BT_Guard"), nullptr, LOAD_NoWarn))
        return Existing;
    auto* BB = Blackboard();
    if (!BB) return nullptr;
    auto* Tree = Asset<UBehaviorTree>(TEXT("BT_Guard")); Tree->BlackboardAsset = BB;
    auto* Root = NewObject<UBTComposite_Selector>(Tree); Tree->RootNode = Root;
    Root->Services.Add(NewObject<UBTService_GuardAwareness>(Tree));
    auto* Chase = NewObject<UBTComposite_Sequence>(Tree); Chase->NodeName = TEXT("Chase visible player");
    Child(Root, Chase, Condition(Tree, TEXT("TargetActor")));
    Child(Chase, Move(Tree, TEXT("TargetActor"))); Child(Chase, Wait(Tree, 0.2f));
    auto* Investigate = NewObject<UBTComposite_Sequence>(Tree); Investigate->NodeName = TEXT("Investigate sprint noise");
    Child(Root, Investigate, Condition(Tree, TEXT("HasNoise")));
    Child(Investigate, Move(Tree, TEXT("NoiseLocation"))); Child(Investigate, Wait(Tree, 1.5f));
    Child(Investigate, NewObject<UBTTask_GuardClearNoise>(Tree));
    auto* Patrol = NewObject<UBTComposite_Sequence>(Tree); Patrol->NodeName = TEXT("Patrol around original spawn");
    Child(Root, Patrol); Child(Patrol, Wait(Tree, 0.8f));
    Child(Patrol, NewObject<UBTTask_GuardPatrolPoint>(Tree)); Child(Patrol, Move(Tree, TEXT("PatrolLocation")));
    // Save the actual editor graph too, so opening the BT cannot replace the runtime tree with an empty graph.
    auto* Graph = CastChecked<UBehaviorTreeGraph>(FBlueprintEditorUtils::CreateNewGraph(Tree, TEXT("Behavior Tree"),
        UBehaviorTreeGraph::StaticClass(), UEdGraphSchema_BehaviorTree::StaticClass()));
    Tree->BTGraph = Graph;
    // Adding each decorator requests a recompile. Wait until every graph node is linked.
    Graph->LockUpdates();
    Graph->GetSchema()->CreateDefaultNodesForGraph(*Graph);
    Graph->OnCreated(); Graph->Initialize();
    // AutoArrange measures Slate widgets and cannot run in a headless commandlet.
    auto Place = [Graph](UBTNode* Instance, int32 X, int32 Y)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
            if (BTNode && BTNode->NodeInstance == Instance)
            { BTNode->NodePosX = X; BTNode->NodePosY = Y; }
        }
    };
    Place(Root, 0, 200);
    for (int32 Branch = 0; Branch < Root->Children.Num(); ++Branch)
    {
        auto* Sequence = Root->Children[Branch].ChildComposite.Get();
        const int32 X = (Branch - 1) * 1800;
        Place(Sequence, X, 500);
        for (int32 Index = 0; Index < Sequence->Children.Num(); ++Index)
            Place(Sequence->Children[Index].ChildTask, X + Index * 400 - 400, 850);
    }
    Graph->UnlockUpdates();
    return Save(Tree) ? Tree : nullptr;
}
UPCGGraph* Props()
{
    if (auto* Existing = LoadObject<UPCGGraph>(nullptr, TEXT("/Game/Homework2/PCG_Courtyard.PCG_Courtyard"), nullptr, LOAD_NoWarn))
        return Existing;
    auto* Graph = Asset<UPCGGraph>(TEXT("PCG_Courtyard"));
    UPCGCreatePointsGridSettings* Grid; auto* G = Graph->AddNodeOfType(Grid);
    Grid->GridExtents = FVector(2600, 2600, 0);
    Grid->CellSize = FVector(650,650,100);
    Grid->CoordinateSpace = EPCGCoordinateSpace::World;
    UPCGTransformPointsSettings* Transform; auto* T = Graph->AddNodeOfType(Transform);
    Transform->OffsetMin = FVector(-120,-120,100); Transform->OffsetMax = FVector(120,120,100);
    Transform->RotationMax = FRotator(0,360,0);
    Transform->bUniformScale = false;
    Transform->ScaleMin = FVector(1.2,1.2,2); Transform->ScaleMax = FVector(2.0,2.0,2);
    UPCGStaticMeshSpawnerSettings* Spawn; auto* S = Graph->AddNodeOfType(Spawn);
    Spawn->bSynchronousLoad = true;
    Spawn->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    auto* Selector = CastChecked<UPCGMeshSelectorWeighted>(Spawn->MeshSelectorParameters);
    auto AddProp = [Selector](const TCHAR* MeshPath, const TCHAR* MaterialPath)
    {
        FPCGMeshSelectorWeightedEntry Entry;
        Entry.Descriptor.StaticMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(MeshPath));
        Entry.Descriptor.OverrideMaterials.Add(TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(MaterialPath)));
        Entry.Descriptor.bUseDefaultCollision = false;
        Entry.Descriptor.BodyInstance.SetCollisionProfileName(TEXT("BlockAll"));
        Entry.Descriptor.bCanEverAffectNavigation = true;
        Selector->MeshEntries.Add(Entry);
    };
    AddProp(TEXT("/Engine/BasicShapes/Cube.Cube"),
        TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark"));
    AddProp(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
        TEXT("/Game/ThirdPerson/MI_ThirdPersonColWay.MI_ThirdPersonColWay"));
    G->SetNodePosition(0,0); T->SetNodePosition(300,0); S->SetNodePosition(600,0);
    Graph->AddEdge(G, TEXT("Out"), T, TEXT("In"));
    Graph->AddEdge(T, TEXT("Out"), S, TEXT("In"));
    Graph->AddEdge(S, TEXT("Out"), Graph->GetOutputNode(), TEXT("Out"));
    return Save(Graph) ? Graph : nullptr;
}
template<class T> T* Volume(UWorld* World, FVector Size)
{
    auto* V = World->SpawnActor<T>();
    auto* Builder = NewObject<UCubeBuilder>();
    Builder->X = Size.X; Builder->Y = Size.Y; Builder->Z = Size.Z;
    UActorFactory::CreateBrushForVolumeActor(V, Builder);
    return V;
}
}

UHomeworkAISetupCommandlet::UHomeworkAISetupCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UHomeworkAISetupCommandlet::Main(const FString& Params)
{
    using namespace HomeworkAISetup;
    auto* Tree = Behavior(); auto* Graph = Props();
    if (!Tree || !Graph) return 1;
    UWorld* World = GEditor->NewMap();
    World->GetWorldSettings()->DefaultGameMode = LoadClass<AGameModeBase>(nullptr,
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode.BP_ThirdPersonGameMode_C"));
    auto* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0,0,-50), FRotator::ZeroRotator);
    Floor->SetActorLabel(TEXT("Courtyard Floor"));
    Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
    Floor->GetStaticMeshComponent()->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr,
        TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray")));
    Floor->SetActorScale3D(FVector(80,80,1));
    Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
    auto* Sun = World->SpawnActor<ADirectionalLight>(FVector(0,0,1000), FRotator(-50,-30,0));
    Sun->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sun->GetLightComponent()->SetIntensity(8.f);
    World->SpawnActor<ASkyAtmosphere>();
    auto* Sky = World->SpawnActor<ASkyLight>();
    Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
    Sky->GetLightComponent()->SetRealTimeCapture(true);
    World->SpawnActor<APlayerStart>(FVector(-3200,0,110), FRotator::ZeroRotator);
    auto* PCG = Volume<APCGVolume>(World, FVector(6000,6000,1000));
    PCG->SetActorLabel(TEXT("PCG Courtyard - change Seed to regenerate"));
    PCG->PCGComponent->Seed = 242;
    PCG->PCGComponent->SetIsPartitioned(false);
    PCG->PCGComponent->SetGraph(Graph);
    PCG->PCGComponent->Generate(true);
    auto* PCGSubsystem = World->GetSubsystem<UPCGSubsystem>();
    const double Deadline = FPlatformTime::Seconds() + 120;
    do
    {
        FTSTicker::GetCoreTicker().Tick(0.016f);
        World->Tick(LEVELTICK_All, 0.016f);
        if (PCGSubsystem) PCGSubsystem->Tick(0.016f);
        FPlatformProcess::Sleep(0.001f);
    } while (PCG->PCGComponent->IsGenerating() && FPlatformTime::Seconds() < Deadline);
    if (PCG->PCGComponent->IsGenerating()) return 1;
    auto* NavBounds = Volume<ANavMeshBoundsVolume>(World, FVector(7900,7900,1000));
    FNavigationSystem::AddNavigationSystemToWorld(*World, FNavigationSystemRunMode::EditorMode);
    auto* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (!Nav) return 2;
    Nav->OnNavigationBoundsUpdated(NavBounds);
    FlushAsyncLoading();
    FAssetCompilingManager::Get().FinishAllCompilation();
    Nav->RemoveNavigationBuildLock(ENavigationBuildLock::AsyncLoadLock,
        UNavigationSystemV1::ELockRemovalRebuildAction::NoRebuild);
    Nav->Tick(0.016f);
    Nav->Build();
    FNavLocation Start;
    if (!Nav->ProjectPointToNavigation(FVector(0,0,100), Start, FVector(100,100,250)))
    {
        UE_LOG(LogTemp, Error, TEXT("No walkable navigation under the guard spawn."));
        return 2;
    }
    for (FVector Location : {FVector(0,0,110), FVector(0,1800,110)})
    {
        auto* Guard = World->SpawnActor<AHomeworkGuard>(Location, FRotator::ZeroRotator);
        Guard->BehaviorTree = Tree;
        Guard->GetMesh()->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr,
            TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed.ABP_Unarmed_C")));
    }
    const FString MapFile = FPackageName::LongPackageNameToFilename(TEXT("/Game/Homework2/Lvl_PCGPatrol"), TEXT(".umap"));
    if (!FEditorFileUtils::SaveLevel(World->PersistentLevel, MapFile)) return 3;
    UE_LOG(LogTemp, Display, TEXT("HW2: saved Blackboard, Behavior Tree, PCG graph and level."));
    return 0;
}
