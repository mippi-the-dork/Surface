// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceProfiling.h"

#include "HAL/IConsoleManager.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceProfile, Log, All);

namespace SurfaceProfiling
{
    namespace
    {
        bool bActive = false;
        FSnapshot Stages[static_cast<int32>(EStage::Count)];
    }

    bool IsActive() { return bActive; }
    void Start()
    {
        Reset();
        bActive = true;
        UE_LOG(LogSurfaceProfile, Display, TEXT("Surface profiling started. Use Surface.Profile.Report to inspect the capture."));
    }
    void Stop()
    {
        bActive = false;
        Report();
    }
    void Reset()
    {
        for (FSnapshot& Stage : Stages)
        {
            Stage = FSnapshot();
        }
    }
    FSnapshot GetSnapshot(EStage Stage)
    {
        const int32 Index = static_cast<int32>(Stage);
        return Index >= 0 && Index < static_cast<int32>(EStage::Count)
            ? Stages[Index] : FSnapshot();
    }
    void Record(EStage Stage, double ElapsedMs)
    {
        if (!bActive || ElapsedMs < 0.0)
        {
            return;
        }
        const int32 Index = static_cast<int32>(Stage);
        if (Index < 0 || Index >= static_cast<int32>(EStage::Count))
        {
            return;
        }
        FSnapshot& Stats = Stages[Index];
        ++Stats.Calls;
        Stats.TotalMs += ElapsedMs;
        Stats.MaximumMs = FMath::Max(Stats.MaximumMs, ElapsedMs);
    }
    void RecordSkippedLayout() { Record(EStage::SkippedExternalLayout, 0.0); }
    const TCHAR* GetStageName(EStage Stage)
    {
        switch (Stage)
        {
        case EStage::Customize: return TEXT("Customize");
        case EStage::ComponentMatching: return TEXT("ComponentMatching");
        case EStage::ExternalLayout: return TEXT("ExternalLayout");
        case EStage::SkippedExternalLayout: return TEXT("SkippedExternalLayout");
        case EStage::FavoriteCollection: return TEXT("FavoriteCollection");
        case EStage::PersistPanelState: return TEXT("PersistPanelState");
        default: return TEXT("Unknown");
        }
    }
    void Report()
    {
        UE_LOG(LogSurfaceProfile, Display, TEXT("Surface Profile | active: %s | cumulative inclusive times (nested stages overlap)"), bActive ? TEXT("yes") : TEXT("no"));
        UE_LOG(LogSurfaceProfile, Display, TEXT("%-24s %10s %12s %12s %12s"), TEXT("Stage"), TEXT("Calls"), TEXT("Total ms"), TEXT("Avg ms"), TEXT("Max ms"));
        for (int32 Index = 0; Index < static_cast<int32>(EStage::Count); ++Index)
        {
            const FSnapshot& Stats = Stages[Index];
            UE_LOG(LogSurfaceProfile, Display, TEXT("%-24s %10llu %12.3f %12.3f %12.3f"),
                GetStageName(static_cast<EStage>(Index)),
                static_cast<unsigned long long>(Stats.Calls), Stats.TotalMs,
                Stats.Calls > 0 ? Stats.TotalMs / static_cast<double>(Stats.Calls) : 0.0,
                Stats.MaximumMs);
        }
        UE_LOG(LogSurfaceProfile, Display, TEXT("ExternalLayout calls count rows created, not properties inside each row."));
    }
}

namespace
{
    FAutoConsoleCommand StartSurfaceProfile(
        TEXT("Surface.Profile.Start"),
        TEXT("Reset and start Surface Details profiling."),
        FConsoleCommandDelegate::CreateStatic(&SurfaceProfiling::Start));
    FAutoConsoleCommand StopSurfaceProfile(
        TEXT("Surface.Profile.Stop"),
        TEXT("Stop Surface Details profiling and print results."),
        FConsoleCommandDelegate::CreateStatic(&SurfaceProfiling::Stop));
    FAutoConsoleCommand ReportSurfaceProfile(
        TEXT("Surface.Profile.Report"),
        TEXT("Print current Surface Details profiling counters."),
        FConsoleCommandDelegate::CreateStatic(&SurfaceProfiling::Report));
    FAutoConsoleCommand ResetSurfaceProfile(
        TEXT("Surface.Profile.Reset"),
        TEXT("Reset Surface Details profiling counters without changing enabled state."),
        FConsoleCommandDelegate::CreateStatic(&SurfaceProfiling::Reset));
}
