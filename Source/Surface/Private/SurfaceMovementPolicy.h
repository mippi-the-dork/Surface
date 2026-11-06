// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
class AActor;
class UObject;
class IPropertyUtilities;

namespace SurfaceMovementPolicy
{
    // Pure decision point so the safety policy is testable without an editor viewport.
    bool ShouldSkipExternalLayout(bool bIsSelectedActorMoving, bool bOptimizationEnabled);
    bool IsLightweightForSelection(const TArray<AActor*>& Actors);
    // Incremented after any actor movement ends, so cached negative Favorites are rescanned.
    uint64 GetFavoritesRefreshRevision();
    void ObserveDetails(const TArray<AActor*>& Actors, const TWeakPtr<IPropertyUtilities>& Utilities);
    void BeginObjectMovement(UObject& Object);
    void EndObjectMovement(UObject& Object);
    void Reset();
}
