// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceProfiling.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceProfilingCountersTest,
    "Surface.Performance.CounterLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceProfilingCountersTest::RunTest(const FString& Parameters)
{
    using namespace SurfaceProfiling;
    Stop();
    Reset();
    Record(EStage::ExternalLayout, 5.0);
    TestEqual(TEXT("Disabled profiling records no samples"), GetSnapshot(EStage::ExternalLayout).Calls, uint64(0));

    Start();
    Record(EStage::ExternalLayout, 2.0);
    Record(EStage::ExternalLayout, 3.0);
    const FSnapshot Snapshot = GetSnapshot(EStage::ExternalLayout);
    TestEqual(TEXT("Two layout samples"), Snapshot.Calls, uint64(2));
    TestEqual(TEXT("Total is accumulated"), Snapshot.TotalMs, 5.0);
    TestEqual(TEXT("Maximum is captured"), Snapshot.MaximumMs, 3.0);
    TestEqual(TEXT("Other stages remain separate"), GetSnapshot(EStage::Customize).Calls, uint64(0));

    Stop();
    Record(EStage::ExternalLayout, 100.0);
    TestEqual(TEXT("Stopped profiling does not change results"), GetSnapshot(EStage::ExternalLayout).Calls, uint64(2));
    Reset();
    TestEqual(TEXT("Reset clears capture"), GetSnapshot(EStage::ExternalLayout).Calls, uint64(0));
    return true;
}
#endif
