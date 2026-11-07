// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceFrozenSnapshot.h"
#include "SurfaceMovementSettings.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceFrozenSnapshotTest,
    "Surface.Performance.FrozenSnapshotPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceFrozenSnapshotTest::RunTest(const FString& Parameters)
{
    const USceneComponent* Component = NewObject<USceneComponent>(GetTransientPackage());
    TArray<SurfaceFrozenSnapshot::FRow> Rows;
    SurfaceFrozenSnapshot::CaptureComponent(Component, Rows);
    TestTrue(TEXT("Concrete component produces safe primitive presentation values"), Rows.Num() > 0);
    for (const SurfaceFrozenSnapshot::FRow& Row : Rows)
    {
        TestTrue(TEXT("Snapshot row has a label"), !Row.Name.IsEmpty());
    }
    SurfaceFrozenSnapshot::CaptureComponent(nullptr, Rows);
    TestEqual(TEXT("Invalid component clears previous presentation values"), Rows.Num(), 0);
    TestNotNull(TEXT("Frozen preview project setting exists"), GetDefault<USurfaceMovementSettings>());
    return true;
}
#endif
