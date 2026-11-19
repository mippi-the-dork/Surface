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


};
