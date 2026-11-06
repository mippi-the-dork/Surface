// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementPolicy.h"
#include "SurfaceMovementSettings.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "IPropertyUtilities.h"

namespace SurfaceMovementPolicy
{
    namespace
    {
        // Only weak references: the Details view and actor must not be kept alive by Surface.
        TSet<TWeakObjectPtr<AActor>> MovingActors;
        uint64 FavoritesRefreshRevision = 0;
        TMap<TWeakObjectPtr<AActor>, TWeakPtr<IPropertyUtilities>> DetailsUtilities;

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
            if (IsValid(Actor)) { DetailsUtilities.Add(Actor, Utilities); }
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
    void Reset()
    {
        MovingActors.Reset();
        DetailsUtilities.Reset();
    }
}
