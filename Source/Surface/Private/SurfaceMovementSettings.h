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

    /** Preserve space below expanded Surface component headers while their live properties are suspended. No UObject property widgets are retained. */
    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Preserve Expanded Panel Space While Moving", EditCondition="bLightweightDuringMovement"))
    bool bPreservePanelSpaceDuringMovement = true;

    /** Approximate height reserved for each expanded component. Tune to match your typical Details content; it is not a live property snapshot. */
    UPROPERTY(Config, EditAnywhere, Category="Performance", meta=(DisplayName="Frozen Component Height (px)", ClampMin="48.0", ClampMax="2000.0", UIMin="80.0", UIMax="800.0", EditCondition="bLightweightDuringMovement && bPreservePanelSpaceDuringMovement"))
    float FrozenComponentHeight = 240.0f;

};
