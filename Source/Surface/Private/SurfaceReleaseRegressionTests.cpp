// Copyright Mippithedork 2026, Inc. All Rights Reserved.
// Release stabilization: exercise real transient Actors/Components, without opening
// an editor tab or changing the user's active level, selection, or asset content.
#include "ComponentDetailsMatching.h"
#include "SurfaceMovementPolicy.h"
#include "SurfaceMovementSettings.h"
#include "SurfacePanelControls.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    USceneComponent* AddTransientComponent(AActor* Actor, const FName Name)
    {
        USceneComponent* Component = NewObject<USceneComponent>(Actor, Name, RF_Transient);
        Actor->AddInstanceComponent(Component);
        return Component;
    }

    FString SingleComponentId(AActor* Actor)
    {
        TArray<Surface::FComponentGroup> Groups;
        int32 Unmatched = 0;
        if (!Surface::BuildComponentGroups({Actor}, Groups, Unmatched) || Groups.Num() != 1)
        {
            return FString();
        }
        return Groups[0].StableId;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceReleaseMovementLifecycle,
    "Surface.Release.Movement.ActorAndComponentLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceReleaseMovementLifecycle::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    if (!TestNotNull(TEXT("Disposable editor world"), World)) { return false; }
    ON_SCOPE_EXIT { SurfaceMovementPolicy::Reset(); World->DestroyWorld(false); };

    AActor* Actor = World->SpawnActor<AActor>();
    AActor* Other = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Actor"), Actor) || !TestNotNull(TEXT("Other"), Other)) { return false; }
    USceneComponent* Component = AddTransientComponent(Actor, TEXT("TransientChild"));
    if (!TestNotNull(TEXT("Component"), Component)) { return false; }

    SurfaceMovementPolicy::Reset();
    const bool bEnabled = GetDefault<USurfaceMovementSettings>()->bLightweightDuringMovement;
    const uint64 Before = SurfaceMovementPolicy::GetFavoritesRefreshRevision();
    TestFalse(TEXT("Actor initially stationary"), SurfaceMovementPolicy::IsLightweightForSelection({Actor}));
    SurfaceMovementPolicy::BeginObjectMovement(*Component);
    TestEqual(TEXT("Component movement applies to its owner"),
        SurfaceMovementPolicy::IsLightweightForSelection({Actor}), bEnabled);
    TestFalse(TEXT("Unrelated Actor remains normal"), SurfaceMovementPolicy::IsLightweightForSelection({Other}));
    SurfaceMovementPolicy::BeginObjectMovement(*Actor);
    SurfaceMovementPolicy::EndObjectMovement(*Component);
    TestFalse(TEXT("Ending movement restores native layout policy"),
        SurfaceMovementPolicy::IsLightweightForSelection({Actor}));
    TestEqual(TEXT("Ending movement invalidates negative Favorites cache"),
        SurfaceMovementPolicy::GetFavoritesRefreshRevision(), Before + uint64(1));
    SurfaceMovementPolicy::EndObjectMovement(*Actor);
    TestEqual(TEXT("Duplicate end cannot invalidate twice"),
        SurfaceMovementPolicy::GetFavoritesRefreshRevision(), Before + uint64(1));
    SurfaceMovementPolicy::BeginObjectMovement(*Actor);
    TestEqual(TEXT("New movement restores lightweight policy"),
        SurfaceMovementPolicy::IsLightweightForSelection({Actor}), bEnabled);
    SurfaceMovementPolicy::Reset();
    TestFalse(TEXT("Shutdown/reset clears active movement state"),
        SurfaceMovementPolicy::IsLightweightForSelection({Actor}));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceReleaseReconstruction,
    "Surface.Release.Components.ReconstructionIdentity",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceReleaseReconstruction::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    if (!TestNotNull(TEXT("Disposable editor world"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* Actor = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("Actor"), Actor)) { return false; }

    USceneComponent* Old = AddTransientComponent(Actor, TEXT("RebuiltComponent"));
    const FString Before = SingleComponentId(Actor);
    TestFalse(TEXT("Original component has a stable group identity"), Before.IsEmpty());
    // Removing a component from InstanceComponents alone does not remove it
    // from the Actor's owned-component set. Surface deliberately enumerates
    // owned components, so a retired object must complete its destruction
    // lifecycle before a replacement is created.
    const bool bRenamed = Old->Rename(TEXT("RetiredRebuiltComponent"), Actor,
        REN_DontCreateRedirectors | REN_NonTransactional);
    if (!TestTrue(TEXT("Retired component can release its original name"), bRenamed)) { return false; }
    Old->DestroyComponent();

    TArray<Surface::FComponentGroup> RetiredGroups;
    int32 RetiredUnmatched = 0;
    TestTrue(TEXT("Actor can be inspected after old component destruction"),
        Surface::BuildComponentGroups({Actor}, RetiredGroups, RetiredUnmatched));
    TestEqual(TEXT("Destroyed component no longer produces a group"), RetiredGroups.Num(), 0);
    TestEqual(TEXT("Destroyed component leaves no unmatched group"), RetiredUnmatched, 0);

    USceneComponent* Replacement = AddTransientComponent(Actor, TEXT("RebuiltComponent"));
    if (!TestNotNull(TEXT("Replacement component"), Replacement)) { return false; }
    const FString After = SingleComponentId(Actor);
    TestEqual(TEXT("Reconstruction preserves component group identity"), After, Before);

    TArray<Surface::FComponentGroup> Groups;
    int32 Unmatched = 0;
    TestTrue(TEXT("Reconstructed actor matches"), Surface::BuildComponentGroups({Actor}, Groups, Unmatched));
    if (TestEqual(TEXT("Exactly one current group"), Groups.Num(), 1))
    {
        TestTrue(TEXT("New component is used, never the retired instance"), Groups[0].Components[0] == Replacement);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceReleaseFavoriteCache,
    "Surface.Release.Favorites.CacheInvalidation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceReleaseFavoriteCache::RunTest(const FString& Parameters)
{
    SurfacePanel::FState State;
    const FString Id = TEXT("StableComponentGroup");
    UClass* Class = USceneComponent::StaticClass();
    TestTrue(TEXT("Unknown favorites require native discovery"), State.NeedsExternalLayout(Id, Class));
    State.RememberFavorites(Id, Class, false);
    TestFalse(TEXT("Collapsed group with confirmed no favorites can skip layout"), State.NeedsExternalLayout(Id, Class));
    State.ExpandedComponents.Add(Id);
    TestTrue(TEXT("Expansion always builds native layout"), State.NeedsExternalLayout(Id, Class));
    State.ExpandedComponents.Remove(Id);
    State.RememberFavorites(Id, Class, true);
    TestTrue(TEXT("Favorites require native layout even while collapsed"), State.NeedsExternalLayout(Id, Class));
    State.RememberFavorites(Id, Class, false);
    TestTrue(TEXT("Different component class invalidates cached result"),
        State.NeedsExternalLayout(Id, UActorComponent::StaticClass()));
    State.InvalidateFavoritePresence();
    TestTrue(TEXT("Manual or metadata refresh rediscovers favorites"), State.NeedsExternalLayout(Id, Class));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceReleaseSelectionPersistence,
    "Surface.Release.Selection.StateAndFiltering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceReleaseSelectionPersistence::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Editor, false);
    if (!TestNotNull(TEXT("Disposable editor world"), World)) { return false; }
    ON_SCOPE_EXIT { World->DestroyWorld(false); };
    AActor* First = World->SpawnActor<AActor>();
    AActor* Second = World->SpawnActor<AActor>();
    if (!TestNotNull(TEXT("First"), First) || !TestNotNull(TEXT("Second"), Second)) { return false; }
    SurfacePanel::FState State;
    State.Actors.Add(First);
    State.Actors.Add(Second);
    TestTrue(TEXT("Selection order does not invalidate a multi-selection"), State.MatchesSelection({Second, First}));
    TestFalse(TEXT("Different selection invalidates state"), State.MatchesSelection({First}));
    SurfacePanel::FComponentItem Item;
    Item.Id = TEXT("StableId");
    Item.Label = FText::FromString(TEXT("Interesting Component"));
    State.AddItem(Item);
    State.SearchQuery = TEXT("interesting");
    TestTrue(TEXT("Search is case insensitive"), State.PassesFilter(Item.Id, Item.Label));
    State.HiddenComponents.Add(Item.Id);
    TestFalse(TEXT("Explicit hidden flag overrides search match"), State.PassesFilter(Item.Id, Item.Label));
    State.HiddenComponents.Remove(Item.Id);
    State.SearchQuery = TEXT("unrelated");
    TestFalse(TEXT("Nonmatching search excludes a component"), State.PassesFilter(Item.Id, Item.Label));
    State.SearchQuery.Reset();
    TestTrue(TEXT("Clearing search restores visibility"), State.PassesFilter(Item.Id, Item.Label));
    return true;
}
#endif
