// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfacePanelControls.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceLazyLayoutPolicyTest,
    "Surface.Performance.LazyLayoutPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceLazyLayoutPolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<SurfacePanel::FState> State = MakeShared<SurfacePanel::FState>();
    const FString Id(TEXT("SampleComponent"));
    UClass* ComponentClass = UActorComponent::StaticClass();
    TestTrue(TEXT("Unknown Favorite state requires native discovery"),
        State->NeedsExternalLayout(Id, ComponentClass));
    State->RememberFavorites(Id, ComponentClass, false);
    TestFalse(TEXT("Collapsed group with no favorites skips layout"),
        State->NeedsExternalLayout(Id, ComponentClass));
    State->ExpandedComponents.Add(Id);
    TestTrue(TEXT("Expanded group always generates editable native details"),
        State->NeedsExternalLayout(Id, ComponentClass));
    State->ExpandedComponents.Remove(Id);
    State->RememberFavorites(Id, ComponentClass, true);
    TestTrue(TEXT("Native favorites always require their layout"),
        State->NeedsExternalLayout(Id, ComponentClass));
    State->RememberFavorites(Id, ComponentClass, false);
    TestTrue(TEXT("Component class replacement invalidates cached absence"),
        State->NeedsExternalLayout(Id, USceneComponent::StaticClass()));
    State->InvalidateFavoritePresence();
    TestTrue(TEXT("Explicit cache invalidation forces rediscovery"),
        State->NeedsExternalLayout(Id, ComponentClass));
    return true;
}
#endif
