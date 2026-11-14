// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceScrollAnchor.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceAnchorRowTest,
    "Surface.Performance.ScrollAnchor.RowMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceAnchorRowTest::RunTest(const FString& Parameters)
{
    using namespace SurfaceScrollAnchor;
    const TArray<FNamedRow> Before = { {20, TEXT("Actor.CustomValue")}, {31, TEXT("Actor.OtherValue")} };
    const FRowAnchor Anchor = CaptureRow(Before, 18.5f);
    TestEqual(TEXT("Anchored property"), Anchor.PropertyPath, FString(TEXT("Actor.CustomValue")));
    const TArray<FNamedRow> During = { {8, TEXT("Actor.CustomValue")}, {19, TEXT("Actor.OtherValue")} };
    const TOptional<float> Adjusted = ResolveOffset(Anchor, During);
    TestTrue(TEXT("Target found despite collapsed Surface rows"), Adjusted.IsSet());
    if (Adjusted.IsSet()) { TestTrue(TEXT("Same offset relative to property"), FMath::IsNearlyEqual(Adjusted.GetValue(), 6.5f)); }
    TestFalse(TEXT("Missing property does not force unrelated scrolling"),
        ResolveOffset(Anchor, TArray<FNamedRow>{ {3, TEXT("OtherActor.Property")} }).IsSet());
    // Above / inside / below Surface: the closest stable Actor property can
    // precede the viewport. Older code only searched forward, causing a
    // distant unrelated anchor to be selected below expanded components.
    const TArray<FNamedRow> Below = {
        { 7, TEXT("Actor.BeforeSurface") },
        { 110, TEXT("Actor.AfterSurface") },
        { 125, TEXT("Actor.FurtherDown") }
    };
    const FRowAnchor BelowAnchor = CaptureRow(Below, 116.25f);
    TestEqual(TEXT("Below Surface chooses nearest actor property"),
        BelowAnchor.PropertyPath, FString(TEXT("Actor.AfterSurface")));
    TestTrue(TEXT("Below Surface remembers negative row offset"),
        FMath::IsNearlyEqual(BelowAnchor.OffsetWithinFirstRow, -6.25f));
    const TArray<FNamedRow> DuringBelow = {
        { 7, TEXT("Actor.BeforeSurface") },
        { 25, TEXT("Actor.AfterSurface") },
        { 40, TEXT("Actor.FurtherDown") }
    };
    const TOptional<float> RestoredBelow = ResolveOffset(BelowAnchor, DuringBelow);
    TestTrue(TEXT("Below Surface anchor resolves after native layout reduction"), RestoredBelow.IsSet());
    if (RestoredBelow.IsSet())
    {
        TestTrue(TEXT("Below Surface preserves the row-relative viewport position"),
            FMath::IsNearlyEqual(RestoredBelow.GetValue(), 31.25f));
    }
    const TArray<FNamedRow> PreferBefore = {
        { 90, TEXT("Actor.LastVisible") },
        { 140, TEXT("Actor.NextFarAway") }
    };
    const FRowAnchor PreviousSide = CaptureRow(PreferBefore, 101.0f);
    TestEqual(TEXT("When next Actor property is distant choose the preceding one"),
        PreviousSide.PropertyPath, FString(TEXT("Actor.LastVisible")));
    return true;
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceAnchorRefreshStabilityTest,
    "Surface.Performance.ScrollAnchor.RefreshStability",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceAnchorRefreshStabilityTest::RunTest(const FString& Parameters)
{
    using namespace SurfaceScrollAnchor;
    FRestoreStability Stability;
    for (int32 Index = 0; Index < 10; ++Index)
    {
        Stability.Observe(123, true, 0.05f);
        ++Stability.SuccessfulRestores;
        Stability.ObserveAlignment(true);
    }
    TestFalse(TEXT("Do not finish before recovery window"), Stability.IsSettled());
    Stability.Observe(456, true, 0.05f);
    TestFalse(TEXT("Late native row rebuild resets settlement"), Stability.IsSettled());
    for (int32 Index = 0; Index < 22; ++Index)
    {
        Stability.Observe(456, true, 0.05f);
        ++Stability.SuccessfulRestores;
        Stability.ObserveAlignment(true);
    }
    TestTrue(TEXT("Stable post-refresh layout completes restoration"), Stability.IsSettled());
    Stability.ObserveAlignment(false);
    TestFalse(TEXT("Pending or clamped table scroll is not successful restoration"), Stability.IsSettled());

    FRestoreStability Missing;
    for (int32 Index = 0; Index < 20; ++Index) { Missing.Observe(0, false, 0.05f); }
    TestFalse(TEXT("Missing anchor never reports restoration complete"), Missing.IsSettled());
    for (int32 Index = 0; Index < 45; ++Index) { Missing.Observe(0, false, 0.05f); }
    TestTrue(TEXT("Missing anchors eventually time out"), Missing.HasTimedOut());
    return true;
}
#endif
