// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SurfaceMovementSettings.generated.h"

/** Settings for the editor-only Surface Details integration. */
UCLASS(Config=EditorPerProjectUserSettings, DefaultConfig, meta=(DisplayName="Surface"))
class USurfaceMovementSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    USurfaceMovementSettings();

    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

    /** Display lightweight Component headers instead of rebuilding component property layouts during actor drags. */
    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Lightweight Details During Actor Movement"))
    bool bLightweightDuringMovement = true;

    /** Experimental legacy geometry placeholder. Disabled by default after inconsistent heights were observed. */
    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Preserve Expanded Panel Space While Moving (Experimental)", AdvancedDisplay, EditCondition="bLightweightDuringMovement"))
    bool bPreservePanelSpaceDuringMovement = false;

    /** Experimental legacy reflection-based preview. Not a snapshot of actual native property rows; disabled by default. */
    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Frozen Property Preview During Actor Movement (Experimental)", AdvancedDisplay, EditCondition="bLightweightDuringMovement"))
    bool bFrozenPropertyPreviewDuringMovement = false;

    // Heights are measured from live Slate geometry, not guessed or user-set.

};
