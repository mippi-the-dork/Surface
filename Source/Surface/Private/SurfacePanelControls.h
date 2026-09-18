// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreGlobals.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "DetailBuilderTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Engine/Level.h"
#include "GameFramework/Actor.h"
#include "IDetailsView.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "IPropertyUtilities.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "UObject/Package.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace SurfacePanel
{
    struct FComponentItem
    {
        FString Id;
        FText Label;
        FText DefaultHiddenReason;
    };

    // Local user preferences for one Actor selection in one Details view.
    // Weak references do not keep Actors or their levels alive.
    struct FState
    {
        TArray<TWeakObjectPtr<AActor>> Actors;
        TArray<FComponentItem> Items;

        FString ConfigSection;
        TSet<FString> KnownComponents;
        TSet<FString> HiddenComponents;
        bool bComponentsExpanded = false;
        bool bFavoritesExpanded = true;

        // Explicitly stores the latest open/closed choice.
        // This is independent of Unreal's custom-node expansion cache.
        TSet<FString> ExpandedComponents;

        FText SearchText;
        FString SearchQuery;

        // Update the config cache immediately. User clicks and committed search
        // changes flush it to disk; typing alone does not write a file per key.
        void Save(bool bFlush = false) const
        {
            if (GConfig == nullptr || GEditorPerProjectIni.IsEmpty() ||
                ConfigSection.IsEmpty())
            {
                return;
            }

            auto SaveSet = [this](const TCHAR* Key, const TSet<FString>& Values)
                {
                    TArray<FString> Sorted = Values.Array();
                    Sorted.Sort();
                    GConfig->SetArray(*ConfigSection, Key, Sorted, GEditorPerProjectIni);
                };

            SaveSet(TEXT("KnownComponents"), KnownComponents);
            SaveSet(TEXT("HiddenComponents"), HiddenComponents);
            SaveSet(TEXT("ExpandedComponents"), ExpandedComponents);
            GConfig->SetString(*ConfigSection, TEXT("Search"),
                *SearchText.ToString(), GEditorPerProjectIni);
            GConfig->SetBool(*ConfigSection, TEXT("ComponentsExpanded"),
                bComponentsExpanded, GEditorPerProjectIni);
            GConfig->SetBool(*ConfigSection, TEXT("FavoritesExpanded"),
                bFavoritesExpanded, GEditorPerProjectIni);

            if (bFlush)
            {
                GConfig->Flush(false, GEditorPerProjectIni);
            }
        }

        void Load()
        {
            if (GConfig == nullptr || GEditorPerProjectIni.IsEmpty())
            {
                return;
            }

            auto LoadSet = [this](const TCHAR* Key, TSet<FString>& Values)
                {
                    TArray<FString> Saved;
                    GConfig->GetArray(*ConfigSection, Key, Saved, GEditorPerProjectIni);
                    Values.Reset();
                    for (const FString& Value : Saved)
                    {
                        Values.Add(Value);
                    }
                };

            LoadSet(TEXT("KnownComponents"), KnownComponents);
            LoadSet(TEXT("HiddenComponents"), HiddenComponents);
            LoadSet(TEXT("ExpandedComponents"), ExpandedComponents);
            FString SavedSearch;
            GConfig->GetString(*ConfigSection, TEXT("Search"),
                SavedSearch, GEditorPerProjectIni);
            SearchText = FText::FromString(SavedSearch);
            SearchQuery = SavedSearch.TrimStartAndEnd();
            GConfig->GetBool(*ConfigSection, TEXT("ComponentsExpanded"),
                bComponentsExpanded, GEditorPerProjectIni);
            GConfig->GetBool(*ConfigSection, TEXT("FavoritesExpanded"),
                bFavoritesExpanded, GEditorPerProjectIni);
        }

        void AddItem(const FComponentItem& Item)
        {
            Items.Add(Item);
            // Defaults apply only the first time this component is encountered.
            // A later refresh must never override an explicit checkbox choice.
            if (!KnownComponents.Contains(Item.Id))
            {
                KnownComponents.Add(Item.Id);
                if (!Item.DefaultHiddenReason.IsEmpty())
                {
                    HiddenComponents.Add(Item.Id);
                }
            }
        }

        bool HasActiveFilters() const
        {
            if (!SearchText.IsEmpty())
            {
                return true;
            }
            for (const FComponentItem& Item : Items)
            {
                if (HiddenComponents.Contains(Item.Id))
                {
                    return true;
                }
            }
            return false;
        }

        void SetVisible(const FString& Id, bool bVisible)
        {
            KnownComponents.Add(Id);
            if (bVisible)
            {
                HiddenComponents.Remove(Id);
            }
            else
            {
                HiddenComponents.Add(Id);
            }
            Save(true);
        }

        void SetAllVisible(bool bVisible)
        {
            for (const FComponentItem& Item : Items)
            {
                KnownComponents.Add(Item.Id);
                if (bVisible)
                {
                    HiddenComponents.Remove(Item.Id);
                }
                else
                {
                    HiddenComponents.Add(Item.Id);
                }
            }
            Save(true);
        }

        void ToggleExpanded(const FString& Id)
        {
            if (ExpandedComponents.Contains(Id))
            {
                ExpandedComponents.Remove(Id);
            }
            else
            {
                ExpandedComponents.Add(Id);
            }
            Save(true);
        }

        bool MatchesSelection(const TArray<AActor*>& InActors) const
        {
            if (Actors.Num() != InActors.Num())
            {
                return false;
            }

            for (AActor* Actor : InActors)
            {
                if (!Actors.Contains(TWeakObjectPtr<AActor>(Actor)))
                {
                    return false;
                }
            }

            return true;
        }

        bool PassesFilter(
            const FString& ComponentId,
            const FText& ComponentLabel) const
        {
            if (HiddenComponents.Contains(ComponentId))
            {
                return false;
            }

            return SearchQuery.IsEmpty() ||
                ComponentLabel.ToString().Contains(
                    SearchQuery,
                    ESearchCase::IgnoreCase);
        }

        int32 GetVisibleCount() const
        {
            int32 Count = 0;

            for (const FComponentItem& Item : Items)
            {
                if (PassesFilter(Item.Id, Item.Label))
                {
                    ++Count;
                }
            }

            return Count;
        }

        void ClearFilters()
        {
            HiddenComponents.Reset();
            SearchText = FText::GetEmpty();
            SearchQuery.Reset();

            for (const FComponentItem& Item : Items)
            {
                KnownComponents.Add(Item.Id);
            }
            // Preserve expansion choices when clearing filters.
            Save(true);
        }
    };

    inline TSharedRef<FState> MakeState(
        const TArray<AActor*>& Actors)
    {
        const TSharedRef<FState> State = MakeShared<FState>();
        State->Actors.Reserve(Actors.Num());

        for (AActor* Actor : Actors)
        {
            State->Actors.Add(TWeakObjectPtr<AActor>(Actor));
        }

        // Actor GUIDs survive ordinary save/reopen and Outliner label changes.
        // Include the level package to distinguish maps and sort for selection order.
        TArray<FString> Keys;
        for (AActor* Actor : Actors)
        {
            if (Actor == nullptr)
            {
                continue;
            }
            const ULevel* Level = Actor->GetLevel();
            const FString LevelName = Level != nullptr
                ? Level->GetOutermost()->GetName() : FString();
            const FString Identity = Actor->GetActorGuid().IsValid()
                ? Actor->GetActorGuid().ToString() : Actor->GetPathName();
            Keys.Add(LevelName + TEXT("|") + Identity);
        }
        Keys.Sort();
        FString Combined;
        for (const FString& Key : Keys)
        {
            Combined += FString::FromInt(Key.Len()) + TEXT(":") + Key;
        }
        State->ConfigSection = TEXT("Surface.Selection.v1.") +
            FMD5::HashAnsiString(*Combined);
        State->Load();
        return State;
    }

    inline TSharedRef<FState> GetState(
        IDetailLayoutBuilder& DetailBuilder,
        const TArray<AActor*>& Actors)
    {
        struct FStoredView
        {
            TWeakPtr<IDetailsView> View;
            TArray<TSharedRef<FState>> SelectionStates;
        };

        // Retain previous selections instead of replacing their preferences
        // whenever the user selects a different Actor.
        static TArray<FStoredView> StoredViews;

        StoredViews.RemoveAll(
            [](const FStoredView& Entry)
            {
                return !Entry.View.IsValid();
            });

        // Release states for deleted Actors and unloaded levels.
        for (FStoredView& Entry : StoredViews)
        {
            Entry.SelectionStates.RemoveAll(
                [](const TSharedRef<FState>& State)
                {
                    if (State->Actors.IsEmpty())
                    {
                        return true;
                    }

                    for (const TWeakObjectPtr<AActor>& Actor : State->Actors)
                    {
                        if (!Actor.IsValid())
                        {
                            return true;
                        }
                    }

                    return false;
                });
        }

        const TSharedPtr<IDetailsView> View =
            DetailBuilder.GetDetailsViewSharedPtr();

        if (!View.IsValid())
        {
            return MakeState(Actors);
        }

        for (FStoredView& Entry : StoredViews)
        {
            if (Entry.View.Pin() != View)
            {
                continue;
            }

            for (const TSharedRef<FState>& State : Entry.SelectionStates)
            {
                if (State->MatchesSelection(Actors))
                {
                    return State;
                }
            }

            const TSharedRef<FState> NewState = MakeState(Actors);
            Entry.SelectionStates.Add(NewState);
            return NewState;
        }

        FStoredView& NewEntry = StoredViews.AddDefaulted_GetRef();
        NewEntry.View = View;

        const TSharedRef<FState> NewState = MakeState(Actors);
        NewEntry.SelectionStates.Add(NewState);
        return NewState;
    }

    inline void ConfigureCategory(
        IDetailCategoryBuilder& Category,
        const TSharedRef<FState>& State,
        bool bFavorites)
    {
        const FText Title = bFavorites
            ? NSLOCTEXT("SurfacePanel", "FavoritesTitle", "Surface - Favorites")
            : NSLOCTEXT("SurfacePanel", "ComponentsTitle", "Surface - Component Details");
        const FText Tip = bFavorites
            ? NSLOCTEXT("SurfacePanel", "FavoritesHeaderTip",
                "Favorite component properties, grouped by component. Favorites apply to the component class and remain visible independently of Surface filters.")
            : NSLOCTEXT("SurfacePanel", "ComponentsHeaderTip",
                "Edit this Actor's component properties. Use the search and Components menu to choose what is shown.");

        Category.HeaderContent(
            SNew(SHorizontalBox)
            .ToolTipText(Tip)
            + SHorizontalBox::Slot()
            .AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 6.0f, 0.0f)
            [
                SNew(SBox).WidthOverride(14.0f).HeightOverride(14.0f)
                    [
                        SNew(SImage)
                            .Image(FAppStyle::GetBrush(bFavorites ? "Icons.Star" : "Icons.Search"))
                            .ColorAndOpacity(FSlateColor::UseForeground())
                    ]
            ]
        + SHorizontalBox::Slot()
            .FillWidth(1.0f).VAlign(VAlign_Center)
            [
                SNew(STextBlock).Text(Title)
                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
                    .ColorAndOpacity(FSlateColor::UseForeground())
            ], true);

        // Surface owns these two outer states, scoped to this Actor selection.
        // Inner native property/category expansion remains managed by Unreal.
        Category.RestoreExpansionState(false);
        Category.InitiallyCollapsed(!(bFavorites
            ? State->bFavoritesExpanded : State->bComponentsExpanded));
        Category.OnExpansionChanged(FOnBooleanValueChanged::CreateLambda(
            [State, bFavorites, CategoryPtr = &Category](bool bExpanded)
            {
                CategoryPtr->InitiallyCollapsed(!bExpanded);
                bool& Stored = bFavorites
                    ? State->bFavoritesExpanded : State->bComponentsExpanded;
                if (Stored != bExpanded)
                {
                    Stored = bExpanded;
                    State->Save(true);
                }
            }));
    }

    inline TSharedRef<SWidget> MakeComponentMenu(
        const TSharedRef<FState>& State)
    {
        const TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);

        Menu->AddSlot()
            .AutoHeight()
            .Padding(4.0f)
            [
                SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                    [
                        SNew(SButton)
                            .Text(NSLOCTEXT(
                                "SurfacePanel",
                                "CheckAll",
                                "Check All"))
                            .ToolTipText(NSLOCTEXT("SurfacePanel", "CheckAllTip",
                                "Check every component, including those normally absent from the component tree. Keep the search text."))
                            .OnClicked_Lambda(
                                [State]()
                                {
                                    State->SetAllVisible(true);
                                    return FReply::Handled();
                                })
                    ]

                + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    [
                        SNew(SButton)
                            .Text(NSLOCTEXT(
                                "SurfacePanel",
                                "UncheckAll",
                                "Uncheck All"))
                            .ToolTipText(NSLOCTEXT("SurfacePanel", "UncheckAllTip",
                                "Hide every component from Component Details. Favorites remain available."))
                            .OnClicked_Lambda(
                                [State]()
                                {
                                    State->SetAllVisible(false);

                                    return FReply::Handled();
                                })
                    ]
            ];

        // List every component, including those hidden by search.
        // Capture item values because the menu outlives this loop.
        for (const FComponentItem& Item : State->Items)
        {
            const FString ComponentId = Item.Id;
            const FText ComponentLabel = Item.Label;

            Menu->AddSlot()
                .AutoHeight()
                .Padding(6.0f, 3.0f)
                [
                    SNew(SCheckBox)
                        .ToolTipText(FText::Format(
                            NSLOCTEXT("SurfacePanel", "ComponentCheckboxTip",
                                "{0}\n{1}\nCheck to include this component. The search filter still applies."),
                            ComponentLabel,
                            !Item.DefaultHiddenReason.IsEmpty()
                            ? Item.DefaultHiddenReason
                            : NSLOCTEXT("SurfacePanel", "NormalComponentTip",
                                "Your visibility choice is saved for this Actor selection.")))
                        .IsChecked_Lambda(
                            [State, ComponentId]()
                            {
                                return State->HiddenComponents.Contains(ComponentId)
                                    ? ECheckBoxState::Unchecked
                                    : ECheckBoxState::Checked;
                            })
                        .OnCheckStateChanged_Lambda(
                            [State, ComponentId](ECheckBoxState NewState)
                            {
                                State->SetVisible(ComponentId,
                                    NewState == ECheckBoxState::Checked);
                            })
                        [
                            SNew(STextBlock)
                                .Text(ComponentLabel)
                                .ToolTipText(ComponentLabel)
                                .Font(IDetailLayoutBuilder::GetDetailFont())
                        ]
                ];
        }

        return SNew(SBox)
            .WidthOverride(380.0f)
            .MaxDesiredHeight(360.0f)
            [
                SNew(SScrollBox)

                    + SScrollBox::Slot()
                    [
                        Menu
                    ]
            ];
    }

    inline void AddControls(
        IDetailCategoryBuilder& Category,
        const TSharedRef<FState>& State,
        const TWeakPtr<IPropertyUtilities>& WeakUtilities)
    {
        // SSearchBox uses InitialText rather than Text_Lambda.
        const TSharedRef<SSearchBox> SearchBox =
            SNew(SSearchBox)
            .HintText(NSLOCTEXT(
                "SurfacePanel",
                "SearchHint",
                "Search components..."))
            .ToolTipText(NSLOCTEXT("SurfacePanel", "SearchTip",
                "Filter by component name or class. Search and checkbox choices are saved for this Actor selection."))
            .InitialText(State->SearchText)
            .DelayChangeNotificationsWhileTyping(false)
            .OnTextChanged_Lambda(
                [State](const FText& NewText)
                {
                    State->SearchText = NewText;
                    State->SearchQuery =
                        NewText.ToString().TrimStartAndEnd();
                    State->Save();
                })
            .OnTextCommitted_Lambda(
                [State](const FText&, ETextCommit::Type)
                {
                    State->Save(true);
                });

        const TWeakPtr<SSearchBox> WeakSearchBox = SearchBox;

        Category.AddCustomRow(
            NSLOCTEXT(
                "SurfacePanel",
                "ControlsSearchText",
                "Surface Component Filters"))
            .WholeRowContent()
            [
                SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                SearchBox
                            ]

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            [
                                SNew(SComboButton)
                                    .ToolTipText(NSLOCTEXT("SurfacePanel", "ComponentsTip",
                                        "Choose which components Surface shows. Unchecked components remain in this list."))
                                    .IsEnabled_Lambda(
                                        [State]()
                                        {
                                            return !State->Items.IsEmpty();
                                        })
                                    .OnGetMenuContent_Lambda(
                                        [State]() -> TSharedRef<SWidget>
                                        {
                                            return MakeComponentMenu(State);
                                        })
                                    .ButtonContent()
                                    [
                                        SNew(STextBlock)
                                            .Text(NSLOCTEXT(
                                                "SurfacePanel",
                                                "Components",
                                                "Components"))
                                            .Font(IDetailLayoutBuilder::GetDetailFont())
                                    ]
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 2.0f)
                    [
                        SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text_Lambda(
                                        [State]()
                                        {
                                            return FText::Format(
                                                NSLOCTEXT(
                                                    "SurfacePanel",
                                                    "VisibleCount",
                                                    "Showing {0} of {1}"),
                                                FText::AsNumber(State->GetVisibleCount()),
                                                FText::AsNumber(State->Items.Num()));
                                        })
                                    .ToolTipText(NSLOCTEXT("SurfacePanel", "CountTip",
                                        "Number of components passing both the search and checkbox filters."))
                                    .Font(IDetailLayoutBuilder::GetDetailFont())
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            .Padding(4.0f, 0.0f)
                            [
                                SNew(SButton)
                                    .Text(NSLOCTEXT(
                                        "SurfacePanel",
                                        "ClearFilters",
                                        "Clear Filters"))
                                    .ToolTipText(NSLOCTEXT("SurfacePanel", "ClearFiltersTip",
                                        "Clear the search and show all components, including those normally absent from the component tree. Keep expansion choices."))
                                    .IsEnabled_Lambda([State]()
                                        { return State->HasActiveFilters(); })
                                    .OnClicked_Lambda(
                                        [State, WeakSearchBox]()
                                        {
                                            State->ClearFilters();

                                            if (const TSharedPtr<SSearchBox> Search =
                                                WeakSearchBox.Pin())
                                            {
                                                Search->SetText(FText::GetEmpty());
                                            }

                                            return FReply::Handled();
                                        })
                            ]

                        + SHorizontalBox::Slot()
                            .AutoWidth()
                            [
                                SNew(SButton)
                                    .Text(NSLOCTEXT(
                                        "SurfacePanel",
                                        "RefreshComponents",
                                        "Refresh Components"))
                                    .ToolTipText(NSLOCTEXT(
                                        "SurfacePanel",
                                        "RefreshComponentsTooltip",
                                        "Refresh the available component list while "
                                        "retaining your filters and expansion choices."))
                                    .OnClicked_Lambda(
                                        [WeakUtilities]()
                                        {
                                            if (const TSharedPtr<IPropertyUtilities> Utilities =
                                                WeakUtilities.Pin())
                                            {
                                                Utilities->RequestForceRefresh();
                                            }

                                            return FReply::Handled();
                                        })
                            ]
                    ]

                + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                            .Text(NSLOCTEXT(
                                "SurfacePanel",
                                "NoFilterMatches",
                                "No components match the current filters."))
                            .Font(IDetailLayoutBuilder::GetDetailFont())
                            .AutoWrapText(true)
                            .Visibility_Lambda(
                                [State]()
                                {
                                    return !State->Items.IsEmpty() &&
                                        State->GetVisibleCount() == 0
                                        ? EVisibility::Visible
                                        : EVisibility::Collapsed;
                                })
                    ]
            ];
    }
}