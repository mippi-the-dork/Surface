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
    }
    TestFalse(TEXT("Do not finish before recovery window"), Stability.IsSettled());
    Stability.Observe(456, true, 0.05f);
    TestFalse(TEXT("Late native row rebuild resets settlement"), Stability.IsSettled());
    for (int32 Index = 0; Index < 10; ++Index)
    {
        Stability.Observe(456, true, 0.05f);
        ++Stability.SuccessfulRestores;
    }
    TestTrue(TEXT("Stable post-refresh layout completes restoration"), Stability.IsSettled());

    FRestoreStability Missing;
    for (int32 Index = 0; Index < 20; ++Index) { Missing.Observe(0, false, 0.05f); }
    TestFalse(TEXT("Missing anchor never reports restoration complete"), Missing.IsSettled());
    for (int32 Index = 0; Index < 45; ++Index) { Missing.Observe(0, false, 0.05f); }
    TestTrue(TEXT("Missing anchors eventually time out"), Missing.HasTimedOut());
    return true;
}
#endif
