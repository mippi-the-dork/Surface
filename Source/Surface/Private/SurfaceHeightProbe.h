// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SBox.h"

/** Zero-content marker used to measure the Slate space occupied by an external Details row.
 *  Markers store only geometry: never UObject references or property handles.
 */
class SSurfaceHeightProbe : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SSurfaceHeightProbe) {}
        SLATE_ARGUMENT(TFunction<void(float)>, OnPosition)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        OnPosition = InArgs._OnPosition;
        ChildSlot[SNew(SBox).HeightOverride(0.0f)];
        // Details rows can be clipped out of the scroll view. Their widget
        // ticks are then unreliable, but an active timer can still sample
        // the layout geometry cached by Slate's arrangement pass.
        RegisterActiveTimer(0.1f, FWidgetActiveTimerDelegate::CreateSP(
            this, &SSurfaceHeightProbe::SampleCachedGeometry));
    }

    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
    {
        SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
        ReportGeometry(AllottedGeometry);
    }

private:
    EActiveTimerReturnType SampleCachedGeometry(double, float)
    {
        // Cached geometry is the last arranged position, not a property handle.
        // It may be unavailable before the first prepass; reject those samples.
        const FGeometry& Geometry = GetCachedGeometry();
        if (Geometry.GetLocalSize().X > KINDA_SMALL_NUMBER)
        {
            ReportGeometry(Geometry);
        }
        return EActiveTimerReturnType::Continue;
    }

    void ReportGeometry(const FGeometry& Geometry)
    {
        if (!OnPosition) { return; }
        const float Scale = Geometry.GetAccumulatedLayoutTransform().GetScale();
        if (Scale > KINDA_SMALL_NUMBER)
        {
            OnPosition(Geometry.GetAbsolutePosition().Y / Scale);
        }
    }

    TFunction<void(float)> OnPosition;
};
