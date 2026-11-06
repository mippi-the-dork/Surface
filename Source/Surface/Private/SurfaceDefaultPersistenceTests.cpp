// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfacePanelControls.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceDefaultPersistencePolicyTest,
    "Surface.Performance.DefaultPersistencePolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceDefaultPersistencePolicyTest::RunTest(const FString& Parameters)
{
    const TSharedRef<SurfacePanel::FState> State = MakeShared<SurfacePanel::FState>();
    SurfacePanel::FComponentItem Item;
    Item.Id = TEXT("SampleComponent");
    Item.Label = FText::FromString(TEXT("Sample Component"));

    TestFalse(TEXT("No defaults are dirty initially"), State->bDiscoveredDefaultsDirty);
    State->AddItem(Item);
    TestTrue(TEXT("New component requires default persistence"), State->bDiscoveredDefaultsDirty);
    TestTrue(TEXT("New defaults are consumed once"), State->SaveDiscoveredDefaultsIfNeeded());
    TestFalse(TEXT("Defaults are clean afterward"), State->bDiscoveredDefaultsDirty);
    State->AddItem(Item);
    TestFalse(TEXT("Repeat discovery must not dirty defaults"), State->bDiscoveredDefaultsDirty);
    TestFalse(TEXT("Unchanged defaults must not be written"), State->SaveDiscoveredDefaultsIfNeeded());

    SurfacePanel::FComponentItem SecondItem;
    SecondItem.Id = TEXT("SecondComponent");
    SecondItem.Label = FText::FromString(TEXT("Second Component"));
    State->AddItem(SecondItem);
    TestTrue(TEXT("New second component dirties defaults"), State->bDiscoveredDefaultsDirty);
    return true;
}
#endif
