// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfacePanelControls.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceMeasuredSectionPolicyTest,
    "Surface.Performance.MeasuredPanelHeightPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceMeasuredSectionPolicyTest::RunTest(const FString& Parameters)
{
    SurfacePanel::FState State;
    TestEqual(TEXT("Unmeasured Component never reserves arbitrary space"),
        State.GetComponentHeight(TEXT("Mesh")), 0.0f);
    TestEqual(TEXT("Unmeasured Favorites never reserves arbitrary space"),
        State.GetFavoritesHeight(), 0.0f);

    auto& Component = State.ComponentHeights.FindOrAdd(TEXT("Mesh"));
    Component.BeginLayout(1);
    Component.Observe(true, 100.0f, 1);
    TestEqual(TEXT("One marker is insufficient"), State.GetComponentHeight(TEXT("Mesh")), 0.0f);
    Component.Observe(false, 425.0f, 1);
    TestEqual(TEXT("Actual component height is remembered"), State.GetComponentHeight(TEXT("Mesh")), 325.0f);
    Component.Observe(false, 90.0f, 1);
    TestEqual(TEXT("Invalid marker ordering does not erase height"), State.GetComponentHeight(TEXT("Mesh")), 325.0f);
    Component.Observe(false, 500.0f, 1);
    TestEqual(TEXT("New layout geometry updates height"), State.GetComponentHeight(TEXT("Mesh")), 400.0f);

    State.FavoritesHeight.BeginLayout(1);
    State.FavoritesHeight.Observe(true, 50.0f, 1);
    State.FavoritesHeight.Observe(false, 250.0f, 1);
    TestEqual(TEXT("Favorites have an independent measured height"), State.GetFavoritesHeight(), 200.0f);
    TestEqual(TEXT("Favorites do not change Component measurements"), State.GetComponentHeight(TEXT("Mesh")), 400.0f);
    Component.BeginLayout(2);
    Component.Observe(true, 110.0f, 2);
    Component.Observe(false, 700.0f, 1); // Old widget callback must be rejected.
    TestEqual(TEXT("Stale marker from previous layout cannot change height"),
        State.GetComponentHeight(TEXT("Mesh")), 400.0f);
    Component.Observe(false, 510.0f, 2);
    TestEqual(TEXT("Matching generation updates measured height"),
        State.GetComponentHeight(TEXT("Mesh")), 400.0f);
    Component.BeginLayout(3);
    Component.Observe(false, 900.0f, 3);
    TestEqual(TEXT("Incomplete generation preserves last valid geometry"),
        State.GetComponentHeight(TEXT("Mesh")), 400.0f);
    return true;
}
#endif
