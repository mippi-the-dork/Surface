// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementSettings.h"
#include "SurfaceMovementPolicy.h"
#include "Components/SceneComponent.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceNativeFreezeBaselineTest,
    "Surface.Performance.NativeFreezeSafeBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceNativeFreezeBaselineTest::RunTest(const FString& Parameters)
{
    const USurfaceMovementSettings* Defaults = GetDefault<USurfaceMovementSettings>();
    TestNotNull(TEXT("Surface movement settings exist"), Defaults);
    if (!Defaults) { return false; }
    // These are class defaults, not a claim about an individual user's saved settings.
    TestTrue(TEXT("Movement layout suppression remains the default"), Defaults->GetClass()->GetDefaultObject<USurfaceMovementSettings>()->bLightweightDuringMovement);
    TestFalse(TEXT("Inaccurate text preview is opt-in only"), Defaults->GetClass()->GetDefaultObject<USurfaceMovementSettings>()->bFrozenPropertyPreviewDuringMovement);
    TestFalse(TEXT("Inexact blank-height preview is opt-in only"), Defaults->GetClass()->GetDefaultObject<USurfaceMovementSettings>()->bPreservePanelSpaceDuringMovement);
    TestTrue(TEXT("Moving actors skip external property layout creation"), SurfaceMovementPolicy::ShouldSkipExternalLayout(true, true));
    TestFalse(TEXT("Normal editing retains native external layouts"), SurfaceMovementPolicy::ShouldSkipExternalLayout(false, true));
    return true;
}
#endif
