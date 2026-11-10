// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

class IDetailsView;
class STableViewBase;

namespace SurfaceScrollAnchor
{
    // A property row is stable across Surface's transient expansion changes.
    // Scroll offsets alone are not: native Slate table offsets are in ROWS.
    struct FRowAnchor
    {
        FString PropertyPath;
        float OffsetWithinFirstRow = 0.0f;
        bool IsValid() const { return !PropertyPath.IsEmpty(); }
    };

    struct FNamedRow
    {
        int32 Index = INDEX_NONE;
        FString Path;
    };

    FRowAnchor CaptureRow(const TArray<FNamedRow>& Rows, float ScrollOffset);
    TOptional<float> ResolveOffset(const FRowAnchor& Anchor, const TArray<FNamedRow>& Rows);

    // Delayed refreshes can continue after OnEndObjectMovement. A restoration
    // session expires only after the observed row mapping has settled.
    struct FRestoreStability
    {
        float ElapsedSeconds = 0.0f;
        float UnchangedSeconds = 0.0f;
        uint32 PreviousLayoutHash = 0;
        bool bHasLayout = false;
        int32 SuccessfulRestores = 0;
        void Observe(uint32 LayoutHash, bool bValidTarget, float DeltaSeconds);
        bool IsSettled() const;
        bool HasTimedOut() const;
    };

    // Returns a weak reference: never takes ownership of native Details widgets.
    TWeakPtr<STableViewBase> FindDetailsTable(const TSharedPtr<IDetailsView>& View);
    TArray<FNamedRow> GetNamedRows(const TSharedPtr<IDetailsView>& View);
}
