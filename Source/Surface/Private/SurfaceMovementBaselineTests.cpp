// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementSettings.h"
#include "SurfaceMovementPolicy.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceMovementBaselineTest,
    "Surface.Performance.MovementSafeBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceMovementBaselineTest::RunTest(const FString& Parameters)
{
    const USurfaceMovementSettings* Defaults = GetDefault<USurfaceMovementSettings>();
    TestNotNull(TEXT("Surface movement settings exist"), Defaults);
    if (!Defaults) { return false; }
    // These are class defaults, not a claim about an individual user's saved settings.
    TestTrue(TEXT("Movement layout suppression remains the default"), Defaults->GetClass()->GetDefaultObject<USurfaceMovementSettings>()->bLightweightDuringMovement);
    TestTrue(TEXT("Moving actors skip external property layout creation"), SurfaceMovementPolicy::ShouldSkipExternalLayout(true, true));
    TestFalse(TEXT("Normal editing retains native external layouts"), SurfaceMovementPolicy::ShouldSkipExternalLayout(false, true));
    return true;
}
#endif
