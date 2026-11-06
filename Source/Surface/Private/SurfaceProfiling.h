// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
class AActor;
class UActorComponent;

namespace SurfaceProfiling
{
    enum class EStage : uint8
    {
        Customize,
        ComponentMatching,
        ExternalLayout,
        SkippedExternalLayout,
        ExpandedGroupLayout,
        CollapsedFavoriteLayout,
        FavoriteCollection,
        PersistPanelState,
        MovementBegin,
        MovementEnd,
        CustomizeDuringMovement,
        ExternalLayoutDuringMovement,
        MovementLayoutSuppressed,
        ComponentInstanceReplaced,
        ComponentInstanceUnchanged,
        Count
    };

    struct FSnapshot
    {
        uint64 Calls = 0;
        double TotalMs = 0.0;
        double MaximumMs = 0.0;
    };

    // Editor/game-thread-only counters. Profiling is disabled unless explicitly started.
    bool IsActive();
    void Start();
    void Stop();
    void Reset();
    FSnapshot GetSnapshot(EStage Stage);
    void Record(EStage Stage, double ElapsedMs);
    void RecordSkippedLayout();
    void BeginObjectMovement(UObject& Object);
    void EndObjectMovement(UObject& Object);
    bool IsMovingActor(AActor* Actor);
    void ObserveComponent(AActor* Actor, const FString& StableId, UActorComponent* Component);
    const TCHAR* GetStageName(EStage Stage);
    void Report();

    class FScopedStage
    {
    public:
        explicit FScopedStage(EStage InStage)
            : Stage(InStage)
            , bMeasure(IsActive())
            , StartSeconds(bMeasure ? FPlatformTime::Seconds() : 0.0)
        {
        }

        ~FScopedStage()
        {
            if (bMeasure)
            {
                Record(Stage, (FPlatformTime::Seconds() - StartSeconds) * 1000.0);
            }
        }

        FScopedStage(const FScopedStage&) = delete;
        FScopedStage& operator=(const FScopedStage&) = delete;

    private:
        EStage Stage;
        bool bMeasure;
        double StartSeconds;
    };
}
