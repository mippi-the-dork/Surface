// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceProfiling.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceMovementProfilingTest,
    "Surface.Performance.MovementInstrumentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceMovementProfilingTest::RunTest(const FString& Parameters)
{
    using namespace SurfaceProfiling;
    Stop();
    Reset();
    USceneComponent* UnownedComponent = NewObject<USceneComponent>(GetTransientPackage());
    TestNotNull(TEXT("Fixture component exists"), UnownedComponent);
    if (UnownedComponent == nullptr) { return false; }

    // Movement callbacks for components without Actors must be harmless.
    BeginObjectMovement(*UnownedComponent);
    EndObjectMovement(*UnownedComponent);
    TestEqual(TEXT("Inactive movement is ignored"), GetSnapshot(EStage::MovementBegin).Calls, uint64(0));

    Start();
    BeginObjectMovement(*UnownedComponent);
    EndObjectMovement(*UnownedComponent);
    ObserveComponent(nullptr, TEXT("Invalid"), UnownedComponent);
    TestEqual(TEXT("Unowned movement is ignored"), GetSnapshot(EStage::MovementBegin).Calls, uint64(0));
    TestEqual(TEXT("Null actor cannot be tracked"), GetSnapshot(EStage::ComponentInstanceReplaced).Calls, uint64(0));
    Record(EStage::CustomizeDuringMovement, 0.0);
    Record(EStage::ExternalLayoutDuringMovement, 0.0);
    TestEqual(TEXT("Movement customization samples"), GetSnapshot(EStage::CustomizeDuringMovement).Calls, uint64(1));
    TestEqual(TEXT("Movement layout samples"), GetSnapshot(EStage::ExternalLayoutDuringMovement).Calls, uint64(1));
    Stop();
    Reset();
    return true;
}
#endif
