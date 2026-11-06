// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementPolicy.h"
#include "SurfaceMovementSettings.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceMovementLightweightPolicyTest,
    "Surface.Performance.MovementLightweightPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceMovementLightweightPolicyTest::RunTest(const FString& Parameters)
{
    using namespace SurfaceMovementPolicy;
    TestFalse(TEXT("Stationary actor builds native layouts"), ShouldSkipExternalLayout(false, true));
    TestFalse(TEXT("Disabled setting preserves native layouts"), ShouldSkipExternalLayout(true, false));
    TestTrue(TEXT("Moving actor skips external layouts"), ShouldSkipExternalLayout(true, true));
    TestNotNull(TEXT("Developer setting exists"), GetDefault<USurfaceMovementSettings>());
    TestFalse(TEXT("No actor selection is never lightweight"), IsLightweightForSelection(TArray<AActor*>()));
    TestTrue(TEXT("Panel-space preservation is available"), GetDefault<USurfaceMovementSettings>()->FrozenComponentHeight >= 48.0f);
    const uint64 RevisionBefore = GetFavoritesRefreshRevision();
    TestEqual(TEXT("Querying favorites revision does not mutate it"), GetFavoritesRefreshRevision(), RevisionBefore);
    return true;
}
#endif
