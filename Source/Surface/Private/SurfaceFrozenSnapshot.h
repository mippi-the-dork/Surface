// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PropertyHandle.h"
#include "UObject/UnrealType.h"

// Text-only movement snapshot. Deliberately contains no UObject or Slate widget references.
namespace SurfaceFrozenSnapshot
{
    struct FRow
    {
        FString Name;
        FString Value;
        FString Category;
    };

    inline bool SupportsProperty(const FProperty* Property)
    {
        return Property && Property->HasAnyPropertyFlags(CPF_Edit) &&
            !Property->HasAnyPropertyFlags(CPF_Deprecated | CPF_DisableEditOnInstance) &&
            (CastField<FBoolProperty>(Property) || CastField<FNumericProperty>(Property) ||
             CastField<FEnumProperty>(Property) || CastField<FNameProperty>(Property) ||
             CastField<FStrProperty>(Property) || CastField<FTextProperty>(Property) ||
             CastField<FObjectPropertyBase>(Property));
    }

    inline void CaptureComponent(const UActorComponent* Component, TArray<FRow>& OutRows)
    {
        OutRows.Reset();
        if (!IsValid(Component) || Component->IsBeingDestroyed()) { return; }
        constexpr int32 MaxRows = 64;
        for (TFieldIterator<FProperty> It(Component->GetClass(), EFieldIteratorFlags::IncludeSuper); It && OutRows.Num() < MaxRows; ++It)
        {
            const FProperty* Property = *It;
            if (!SupportsProperty(Property)) { continue; }
            FRow Row;
            Row.Name = Property->GetDisplayNameText().ToString();
            Row.Category = Property->GetMetaData(TEXT("Category"));
            const void* Address = Property->ContainerPtrToValuePtr<void>(Component);
            Property->ExportTextItem_Direct(Row.Value, Address, nullptr, const_cast<UActorComponent*>(Component), PPF_None);
            if (Row.Value.Len() > 160) { Row.Value.LeftInline(157); Row.Value += TEXT("..."); }
            OutRows.Add(MoveTemp(Row));
        }
    }

    inline void CaptureFavorite(const TSharedRef<IPropertyHandle>& Handle, TArray<FRow>& OutRows)
    {
        if (OutRows.Num() >= 64 || !Handle->IsValidHandle()) { return; }
        FRow Row;
        Row.Name = Handle->GetPropertyDisplayName().ToString();
        FText DisplayValue;
        if (Handle->GetValueAsDisplayText(DisplayValue) == FPropertyAccess::Success)
        {
            Row.Value = DisplayValue.ToString();
        }
        else
        {
            Row.Value = TEXT("<Multiple or unavailable values>");
        }
        if (Row.Value.Len() > 160) { Row.Value.LeftInline(157); Row.Value += TEXT("..."); }
        OutRows.Add(MoveTemp(Row));
    }
}
