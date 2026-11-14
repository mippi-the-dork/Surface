// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceScrollAnchor.h"

#include "IDetailsView.h"
#include "PropertyPath.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "Widgets/SWidget.h"
#include "Widgets/Views/STableViewBase.h"

namespace SurfaceScrollAnchor
{
    FRowAnchor CaptureRow(const TArray<FNamedRow>& Rows, float ScrollOffset)
    {
        FRowAnchor Result;
        const FNamedRow* Before = nullptr;
        const FNamedRow* After = nullptr;
        // An Actor property can be BELOW the viewport when Surface rows
        // occupy the screen. Never assume that the first later property is
        // the one the user is looking at. Keep the closest property on
        // either side, including the signed relative distance in rows.
        for (const FNamedRow& Row : Rows)
        {
            if (Row.Path.IsEmpty() || Row.Index < 0) { continue; }
            if (static_cast<float>(Row.Index) <= ScrollOffset)
            {
                if (!Before || Row.Index > Before->Index) { Before = &Row; }
            }
            else if (!After || Row.Index < After->Index)
            {
                After = &Row;
            }
        }
        const FNamedRow* Best = nullptr;
        if (Before && After)
        {
            const float DistanceBefore = ScrollOffset - Before->Index;
            const float DistanceAfter = After->Index - ScrollOffset;
            Best = (DistanceBefore <= DistanceAfter) ? Before : After;
        }
        else { Best = Before ? Before : After; }
        if (Best)
        {
            Result.PropertyPath = Best->Path;
            Result.OffsetWithinFirstRow = static_cast<float>(Best->Index) - ScrollOffset;
        }
        return Result;
    }

    TOptional<float> ResolveOffset(const FRowAnchor& Anchor, const TArray<FNamedRow>& Rows)
    {
        if (!Anchor.IsValid()) { return {}; }
        for (const FNamedRow& Row : Rows)
        {
            if (Row.Path == Anchor.PropertyPath)
            {
                return FMath::Max(0.0f, static_cast<float>(Row.Index) - Anchor.OffsetWithinFirstRow);
            }
        }
        return {};
    }

    void FRestoreStability::Observe(uint32 LayoutHash, bool bValidTarget, float DeltaSeconds)
    {
        ElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
        if (!bValidTarget)
        {
            bHasLayout = false;
            bObservedAlignment = false;
            UnchangedSeconds = 0.0f;
            return;
        }
        if (!bHasLayout || PreviousLayoutHash != LayoutHash)
        {
            PreviousLayoutHash = LayoutHash;
            UnchangedSeconds = 0.0f;
            bObservedAlignment = false;
            bHasLayout = true;
        }
        else
        {
            UnchangedSeconds += FMath::Max(0.0f, DeltaSeconds);
        }
    }

    bool FRestoreStability::IsSettled() const
    {
        // Require a post-release recovery interval AND a stable property-row
        // mapping. 12 arbitrary ticker updates were insufficient in Alpha 12.
        return bHasLayout && bObservedAlignment && SuccessfulRestores > 0 &&
            ElapsedSeconds >= 1.25f && UnchangedSeconds >= 0.35f;
    }

    bool FRestoreStability::HasTimedOut() const
    {
        return ElapsedSeconds >= 3.0f;
    }

    static TSharedPtr<STableViewBase> FindTableRecursive(const TSharedRef<SWidget>& Widget, int32 Depth)
    {
        if (Depth > 32) { return nullptr; }
        // The main Details list is an STreeView. Only cast after checking the
        // Slate widget's declared type; other nested widget types are ignored.
        if (Widget->GetType() == FName(TEXT("STreeView")))
        {
            return StaticCastSharedRef<STableViewBase>(Widget);
        }
        FChildren* Children = Widget->GetChildren();
        if (!Children) { return nullptr; }
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<STableViewBase> Match = FindTableRecursive(Children->GetChildAt(Index), Depth + 1))
            {
                return Match;
            }
        }
        return nullptr;
    }

    TWeakPtr<STableViewBase> FindDetailsTable(const TSharedPtr<IDetailsView>& View)
    {
        if (!View.IsValid()) { return {}; }
        return FindTableRecursive(View.ToSharedRef(), 0);
    }

    TArray<FNamedRow> GetNamedRows(const TSharedPtr<IDetailsView>& View)
    {
        TArray<FNamedRow> Rows;
        if (!View.IsValid()) { return Rows; }
        for (const TPair<int32, FPropertyPath>& Entry : View->GetPropertyRowNumbers())
        {
            if (!Entry.Value.IsValid()) { continue; }
            // Anchor to actor-owned properties only. External component rows
            // disappear during movement and cannot serve as stable anchors.
            const FProperty* RootProperty = Entry.Value.GetRootProperty().Property.Get();
            const UClass* OwnerClass = RootProperty ? Cast<UClass>(RootProperty->GetOwnerStruct()) : nullptr;
            if (OwnerClass && OwnerClass->IsChildOf(AActor::StaticClass()))
            {
                Rows.Add({ Entry.Key, Entry.Value.ToString(TEXT(".")) });
            }
        }
        return Rows;
    }
}
