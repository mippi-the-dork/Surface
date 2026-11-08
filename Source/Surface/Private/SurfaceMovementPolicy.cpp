// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementPolicy.h"
#include "SurfaceMovementSettings.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "IPropertyUtilities.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorMetadataOverrides.h"

namespace SurfaceMovementPolicy
{
    namespace
    {
        // Only weak references: the Details view and actor must not be kept alive by Surface.
        TSet<TWeakObjectPtr<AActor>> MovingActors;
        uint64 FavoritesRefreshRevision = 0;
        TMap<TWeakObjectPtr<AActor>, TWeakPtr<IPropertyUtilities>> DetailsUtilities;
        // Metadata is a list of paths, not a live property handle. A class-wide
        // snapshot detects favorites changed in other Details views.
        TMap<TWeakObjectPtr<UClass>, TArray<FString>> FavoritesMetadata;

        bool ReadFavorites(UClass* Class, TArray<FString>& OutPaths)
        {
            OutPaths.Reset();
            if (!Class || !GEditor) { return false; }
            UEditorMetadataOverrides* Overrides = GEditor->GetEditorSubsystem<UEditorMetadataOverrides>();
            if (!Overrides) { return false; }
            Overrides->GetArrayMetadata(Class, FName(TEXT("FavoriteProperties")), OutPaths);
            OutPaths.Sort();
            return true;
        }

        void ObserveClasses(AActor* Actor)
        {
            if (!IsValid(Actor)) { return; }
            TInlineComponentArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (!IsValid(Component)) { continue; }
                UClass* Class = Component->GetClass();
                if (!FavoritesMetadata.Contains(Class))
                {
                    TArray<FString> Paths;
                    if (ReadFavorites(Class, Paths))
                    {
                        FavoritesMetadata.Add(Class, MoveTemp(Paths));
                    }
                }
            }
        }

        AActor* GetActor(UObject& Object)
        {
            if (AActor* Actor = Cast<AActor>(&Object)) { return Actor; }
            if (UActorComponent* Component = Cast<UActorComponent>(&Object)) { return Component->GetOwner(); }
            return nullptr;
        }
        void Refresh(AActor* Actor)
        {
            if (!Actor) { return; }
            if (const TWeakPtr<IPropertyUtilities>* Weak = DetailsUtilities.Find(Actor))
            {
                if (TSharedPtr<IPropertyUtilities> Utilities = Weak->Pin())
                {
                    // Request, rather than synchronous force, avoids rebuilding inside editor delegates.
                    Utilities->RequestForceRefresh();
                }
            }
        }
    }

    bool ShouldSkipExternalLayout(bool bIsSelectedActorMoving, bool bOptimizationEnabled)
    {
        return bIsSelectedActorMoving && bOptimizationEnabled;
    }
    uint64 GetFavoritesRefreshRevision() { return FavoritesRefreshRevision; }
    bool IsLightweightForSelection(const TArray<AActor*>& Actors)
    {
        const bool bEnabled = GetDefault<USurfaceMovementSettings>()->bLightweightDuringMovement;
        if (!bEnabled) { return false; }
        for (AActor* Actor : Actors)
        {
            if (Actor && MovingActors.Contains(Actor)) { return true; }
        }
        return false;
    }
    void ObserveDetails(const TArray<AActor*>& Actors, const TWeakPtr<IPropertyUtilities>& Utilities)
    {
        if (!Utilities.IsValid()) { return; }
        for (AActor* Actor : Actors)
        {
            if (IsValid(Actor)) { DetailsUtilities.Add(Actor, Utilities); ObserveClasses(Actor); }
        }
        // Bound memory growth for long sessions with many different selections.
        for (auto It = DetailsUtilities.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); }
        }
    }
    void BeginObjectMovement(UObject& Object)
    {
        AActor* Actor = GetActor(Object);
        if (!IsValid(Actor)) { return; }
        if (!MovingActors.Contains(Actor))
        {
            MovingActors.Add(Actor);
            if (GetDefault<USurfaceMovementSettings>()->bLightweightDuringMovement) { Refresh(Actor); }
        }
    }
    void EndObjectMovement(UObject& Object)
    {
        AActor* Actor = GetActor(Object);
        if (!Actor) { return; }
        if (MovingActors.Remove(Actor) > 0)
        {
            // Invalidate session-only negative Favorites information after reconstruction.
            ++FavoritesRefreshRevision;
            // Rebuild with new UObject instances, never reuse pre-reconstruction handles.
            Refresh(Actor);
        }
    }
    bool PollFavoritesMetadata(float DeltaTime)
    {
        // The poll is intentionally cheap: one metadata read per observed class,
        // without asking PropertyEditor to build or walk a single property row.
        TSet<UClass*> ChangedClasses;
        for (auto It = FavoritesMetadata.CreateIterator(); It; ++It)
        {
            UClass* Class = It.Key().Get();
            if (!IsValid(Class)) { It.RemoveCurrent(); continue; }
            TArray<FString> Current;
            if (ReadFavorites(Class, Current) && Current != It.Value())
            {
                It.Value() = MoveTemp(Current);
                ChangedClasses.Add(Class);
            }
        }
        if (ChangedClasses.IsEmpty()) { return true; }
        ++FavoritesRefreshRevision;

        for (auto It = DetailsUtilities.CreateIterator(); It; ++It)
        {
            AActor* Actor = It.Key().Get();
            if (!IsValid(Actor) || !It.Value().IsValid())
            {
                It.RemoveCurrent();
                continue;
            }
            // Never rebuild Surface's external rows in the middle of a drag.
            // EndObjectMovement already schedules a safe refresh afterward.
            if (MovingActors.Contains(Actor)) { continue; }
            TInlineComponentArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (IsValid(Component) && ChangedClasses.Contains(Component->GetClass()))
                {
                    Refresh(Actor);
                    break;
                }
            }
        }
        return true;
    }
    void Reset()
    {
        MovingActors.Reset();
        DetailsUtilities.Reset();
        FavoritesMetadata.Reset();
    }
}
