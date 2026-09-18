// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentDetailsMatching.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    template <typename ComponentType>
    ComponentType* AddTestComponent(AActor* Actor, const TCHAR* Name)
    {
        ComponentType* Component = NewObject<ComponentType>(Actor, FName(Name), RF_Transient);
        Actor->AddInstanceComponent(Component);
        // Rendering/physics registration is unnecessary for this matching test.
        return Component;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceMatchingTest,
    "Surface.Matching.SelectionSafety",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceMatchingTest::RunTest(const FString& Parameters)
{
    // A disposable world keeps the user's current level and selection untouched.
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    if (!TestNotNull(TEXT("Disposable test world"), World))
    {
        return false;
    }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };

    AActor* First = World->SpawnActor<AActor>();
    AActor* Second = World->SpawnActor<AActor>();
    AActor* Third = World->SpawnActor<AActor>();
    AStaticMeshActor* DifferentActorClass = World->SpawnActor<AStaticMeshActor>();
    if (!TestNotNull(TEXT("First Actor"), First) ||
        !TestNotNull(TEXT("Second Actor"), Second) ||
        !TestNotNull(TEXT("Third Actor"), Third) ||
        !TestNotNull(TEXT("Different Actor class"), DifferentActorClass))
    {
        return false;
    }

    USceneComponent* FirstLeft = AddTestComponent<USceneComponent>(First, TEXT("Left"));
    AddTestComponent<USceneComponent>(First, TEXT("Right"));
    // Reverse creation order deliberately: array-order matching would be unsafe.
    AddTestComponent<USceneComponent>(Second, TEXT("Right"));
    USceneComponent* SecondLeft = AddTestComponent<USceneComponent>(Second, TEXT("Left"));
    AddTestComponent<UStaticMeshComponent>(Third, TEXT("Left"));

    TArray<Surface::FComponentGroup> Groups;
    int32 UnmatchedCount = 0;
    TestTrue(TEXT("Equivalent Actors accepted"),
        Surface::BuildComponentGroups({First, Second}, Groups, UnmatchedCount));
    TestEqual(TEXT("Both named components matched"), Groups.Num(), 2);
    TestEqual(TEXT("No incomplete matches"), UnmatchedCount, 0);
    for (const Surface::FComponentGroup& Group : Groups)
    {
        TestEqual(TEXT("One object per selected Actor"), Group.Components.Num(), 2);
        if (Group.Components.Num() == 2)
        {
            TestEqual(TEXT("Names match despite reversed creation order"),
                Group.Components[0]->GetFName(), Group.Components[1]->GetFName());
            TestTrue(TEXT("Objects belong to different Actors"),
                Group.Components[0]->GetOwner() != Group.Components[1]->GetOwner());
        }
    }

    AddTestComponent<USceneComponent>(First, TEXT("OnlyOnFirst"));
    TestTrue(TEXT("Missing components handled"),
        Surface::BuildComponentGroups({First, Second}, Groups, UnmatchedCount));
    TestEqual(TEXT("Incomplete match omitted"), Groups.Num(), 2);
    TestEqual(TEXT("Incomplete match reported"), UnmatchedCount, 1);

    TestTrue(TEXT("Different component classes handled"),
        Surface::BuildComponentGroups({First, Third}, Groups, UnmatchedCount));
    TestEqual(TEXT("Same name with different class is not matched"), Groups.Num(), 0);

    SecondLeft->CreationMethod = EComponentCreationMethod::SimpleConstructionScript;
    TestTrue(TEXT("Creation-method mismatch handled"),
        Surface::BuildComponentGroups({First, Second}, Groups, UnmatchedCount));
    TestEqual(TEXT("Only Right remains a full match"), Groups.Num(), 1);
    SecondLeft->CreationMethod = FirstLeft->CreationMethod;

    TestFalse(TEXT("Mixed Actor classes rejected"),
        Surface::BuildComponentGroups({First, DifferentActorClass}, Groups, UnmatchedCount));
    TestEqual(TEXT("Invalid selection produces no edit groups"), Groups.Num(), 0);
    TestFalse(TEXT("Duplicate Actors rejected"),
        Surface::BuildComponentGroups({First, First}, Groups, UnmatchedCount));
    TestFalse(TEXT("Null Actor rejected"),
        Surface::BuildComponentGroups({First, nullptr}, Groups, UnmatchedCount));
    TestFalse(TEXT("Empty selection rejected"),
        Surface::BuildComponentGroups({}, Groups, UnmatchedCount));
    return true;
}

#endif
