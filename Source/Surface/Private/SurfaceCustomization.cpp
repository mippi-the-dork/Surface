#include "SurfaceCustomization.h"

#include "ComponentDetailsMatching.h"
#include "SurfacePanelControls.h"

#include "BlueprintEditorSettings.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "DetailBuilderTypes.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "IPropertyUtilities.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Logging/LogMacros.h"
#include "Misc/Attribute.h"
#include "Misc/ScopeExit.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "Surface"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceFavorites, Log, All);

namespace
{
    // Prevent embedded component layouts from creating another Surface section.
    thread_local bool GBuildingComponentDetails = false;

    /**
     * Temporary collection for one embedded component group.
     *
     * The component customization registers a callback that reads its native
     * Favorites category after the layout has been generated. It does not call
     * EditCategory("Favorites"), which would change the category's ownership.
     */
    struct FSurfaceFavoriteCapture
    {
        explicit FSurfaceFavoriteCapture(
            const TArray<UObject*>& InObjects)
        {
            Objects.Reserve(InObjects.Num());

            for (UObject* Object : InObjects)
            {
                Objects.Add(TWeakObjectPtr<UObject>(Object));
            }
        }

        TArray<TWeakObjectPtr<UObject>> Objects;
        TArray<TSharedRef<IPropertyHandle>> Properties;

        bool bCallbackRegistered = false;
        bool bCollected = false;
    };

    thread_local TSharedPtr<FSurfaceFavoriteCapture> GFavoriteCapture;

    bool RegisterComponentFavoriteCapture(
        IDetailLayoutBuilder& DetailBuilder)
    {
        if (!GFavoriteCapture.IsValid())
        {
            return false;
        }

        TArray<TWeakObjectPtr<UObject>> Objects;
        DetailBuilder.GetObjectsBeingCustomized(Objects);

        // Only inspect the component group currently being embedded.
        // Ignore any other nested layouts created by component customizations.
        if (Objects.IsEmpty() ||
            Objects.Num() != GFavoriteCapture->Objects.Num())
        {
            return false;
        }

        for (const TWeakObjectPtr<UObject>& WeakObject : Objects)
        {
            UObject* Object = WeakObject.Get();

            if (!IsValid(Object) ||
                !Object->IsA<UActorComponent>() ||
                !GFavoriteCapture->Objects.Contains(WeakObject))
            {
                return false;
            }
        }

        if (GFavoriteCapture->bCallbackRegistered)
        {
            return true;
        }

        GFavoriteCapture->bCallbackRegistered = true;

        const TWeakPtr<FSurfaceFavoriteCapture> WeakCapture =
            GFavoriteCapture;

        // In this engine version, the category-sort callbacks run after
        // category generation. That gives us the completed native Favorites
        // category through a public interface, without editing that category.
        //
        // This callback only reads properties. It does not change sorting
        // or add rows to the component's layout.
        DetailBuilder.SortCategories(
            [WeakCapture](
                const TMap<FName, IDetailCategoryBuilder*>& Categories)
            {
                const TSharedPtr<FSurfaceFavoriteCapture> Capture =
                    WeakCapture.Pin();

                if (!Capture.IsValid())
                {
                    return;
                }

                Capture->Properties.Reset();
                Capture->bCollected = true;

                IDetailCategoryBuilder* const* FavoritesEntry =
                    Categories.Find(FName(TEXT("Favorites")));

                if (FavoritesEntry == nullptr ||
                    *FavoritesEntry == nullptr)
                {
                    // No native Favorites category, such as when the editor's
                    // favorites system is disabled.
                    return;
                }

                TArray<TSharedRef<IPropertyHandle>> NativeFavorites;

                (*FavoritesEntry)->GetDefaultProperties(
                    NativeFavorites,
                    true,
                    true);

                for (const TSharedRef<IPropertyHandle>& Handle : NativeFavorites)
                {
                    if (!Handle->IsValidHandle() ||
                        Handle->GetProperty() == nullptr ||
                        !Handle->IsFavorite())
                    {
                        continue;
                    }

                    // Confirm the handle belongs to this component selection.
                    TArray<UObject*> OuterObjects;
                    Handle->GetOuterObjects(OuterObjects);

                    if (OuterObjects.Num() != Capture->Objects.Num())
                    {
                        continue;
                    }

                    bool bMatchesComponents = true;

                    for (UObject* OuterObject : OuterObjects)
                    {
                        if (!Capture->Objects.Contains(
                            TWeakObjectPtr<UObject>(OuterObject)))
                        {
                            bMatchesComponents = false;
                            break;
                        }
                    }

                    if (!bMatchesComponents)
                    {
                        continue;
                    }

                    const bool bAlreadyCollected =
                        Capture->Properties.ContainsByPredicate(
                            [&Handle](
                                const TSharedRef<IPropertyHandle>& Existing)
                            {
                                return Existing->IsSamePropertyNode(Handle);
                            });

                    if (!bAlreadyCollected)
                    {
                        Capture->Properties.Add(Handle);
                    }
                }
            });

        return true;
    }

    // Match the stock UE 5.8 level-instance component-tree inclusion rules.
    // This controls only the initial Surface checkbox, not property editability.
    // Subsystem adapters and other plugins can apply additional tree filtering.
    FText GetDefaultHiddenReason(
        UActorComponent& Component,
        bool bHideConstructionScriptComponents)
    {
        const AActor* Owner = Component.GetOwner();
        if (Owner != nullptr && Owner->GetRootComponent() == &Component)
        {
            // GatherSubobjectData adds the root unconditionally after filtering.
            return FText::GetEmpty();
        }

        if (Component.IsVisualizationComponent())
        {
            return LOCTEXT("HiddenVisualization",
                "Visualization helper, normally absent from the component tree.");
        }
        if (Component.CreationMethod == EComponentCreationMethod::UserConstructionScript &&
            bHideConstructionScriptComponents)
        {
            return LOCTEXT("HiddenConstructionScript",
                "Hidden from the component tree by your construction-script component setting.");
        }

        const USceneComponent* SceneComponent = Cast<USceneComponent>(&Component);
        const USceneComponent* Parent = SceneComponent != nullptr
            ? SceneComponent->GetAttachParent() : nullptr;
        if (Parent != nullptr && Parent->IsCreatedByConstructionScript() &&
            Component.HasAnyFlags(RF_DefaultSubObject))
        {
            return LOCTEXT("HiddenNestedDefaultSubobject",
                "Nested default subobject under a construction-script component, normally absent from the component tree.");
        }
        if (Component.CreationMethod == EComponentCreationMethod::Native &&
            FComponentEditorUtils::GetPropertyForEditableNativeComponent(&Component) == nullptr)
        {
            return LOCTEXT("HiddenNativeComponent",
                "Native component without an editable component property, normally absent from the component tree.");
        }
        return FText::GetEmpty();
    }

    void SelectSurfaceObjects(
        const TArray<TWeakObjectPtr<UActorComponent>>& Components,
        bool bSelectActorsOnly)
    {
        if (GEditor == nullptr || Components.IsEmpty())
        {
            return;
        }

        TArray<TWeakObjectPtr<AActor>> Owners;

        // Validate the complete selection before changing editor state.
        for (const TWeakObjectPtr<UActorComponent>& WeakComponent : Components)
        {
            UActorComponent* Component = WeakComponent.Get();

            if (!IsValid(Component) ||
                Component->IsBeingDestroyed() ||
                Component->IsTemplate())
            {
                return;
            }

            AActor* Owner = Component->GetOwner();

            if (!IsValid(Owner) ||
                Owner->IsActorBeingDestroyed() ||
                Owner->IsTemplate())
            {
                return;
            }

            const UWorld* World = Owner->GetWorld();

            if (World == nullptr ||
                World->WorldType != EWorldType::Editor)
            {
                return;
            }

            if (!GEditor->CanSelectActor(Owner, true, false, true))
            {
                return;
            }

            Owners.AddUnique(TWeakObjectPtr<AActor>(Owner));
        }

        GEditor->SelectNone(false, true, false);

        for (const TWeakObjectPtr<AActor>& WeakOwner : Owners)
        {
            if (AActor* Owner = WeakOwner.Get())
            {
                if (!Owner->IsActorBeingDestroyed())
                {
                    GEditor->SelectActor(
                        Owner,
                        true,
                        false,
                        false,
                        false);
                }
            }
        }

        if (!bSelectActorsOnly)
        {
            for (const TWeakObjectPtr<UActorComponent>& WeakComponent : Components)
            {
                if (UActorComponent* Component = WeakComponent.Get())
                {
                    if (!Component->IsBeingDestroyed())
                    {
                        GEditor->SelectComponent(
                            Component,
                            true,
                            false,
                            false);
                    }
                }
            }
        }

        GEditor->NoteSelectionChange(true);
        GEditor->RedrawLevelEditingViewports(true);
    }

    void QueueSurfaceSelection(
        const TArray<TWeakObjectPtr<UActorComponent>>& Components,
        const TWeakPtr<IPropertyUtilities>& WeakUtilities,
        bool bSelectActorsOnly)
    {
        if (const TSharedPtr<IPropertyUtilities> Utilities =
            WeakUtilities.Pin())
        {
            // Selection can destroy the originating widgets.
            Utilities->EnqueueDeferredAction(
                FSimpleDelegate::CreateLambda(
                    [Components, bSelectActorsOnly]()
                    {
                        SelectSurfaceObjects(
                            Components,
                            bSelectActorsOnly);
                    }));
        }
    }

    void PositionSurfaceCategories(
        IDetailLayoutBuilder& DetailBuilder)
    {
        DetailBuilder.SortCategories(
            [](const TMap<FName, IDetailCategoryBuilder*>& Categories)
            {
                TArray<IDetailCategoryBuilder*> SurfaceCategories;

                auto AddSurfaceCategory =
                    [&Categories, &SurfaceCategories](FName Name)
                    {
                        IDetailCategoryBuilder* const* Entry =
                            Categories.Find(Name);

                        if (Entry != nullptr && *Entry != nullptr)
                        {
                            SurfaceCategories.Add(*Entry);
                        }
                    };

                // The Actor view shows these two in this order.
                AddSurfaceCategory(FName(TEXT("Surface_Favorites")));
                AddSurfaceCategory(FName(TEXT("Surface_Components")));

                // The direct component view shows only the return section.
                AddSurfaceCategory(FName(TEXT("Surface_Selection")));

                if (SurfaceCategories.IsEmpty())
                {
                    return;
                }

                IDetailCategoryBuilder* const* TransformEntry =
                    Categories.Find(FName(TEXT("TransformCommon")));

                if (TransformEntry == nullptr || *TransformEntry == nullptr)
                {
                    TransformEntry =
                        Categories.Find(FName(TEXT("Transform")));
                }

                if (TransformEntry == nullptr || *TransformEntry == nullptr)
                {
                    return;
                }

                IDetailCategoryBuilder* TransformCategory = *TransformEntry;

                const int32 TransformOrder =
                    TransformCategory->GetSortOrder();

                const int32 InsertCount = SurfaceCategories.Num();

                if (TransformOrder >= MAX_int32 - InsertCount)
                {
                    return;
                }

                // Preserve other categories' relative order.
                for (const TPair<FName, IDetailCategoryBuilder*>& Entry : Categories)
                {
                    IDetailCategoryBuilder* OtherCategory = Entry.Value;

                    if (OtherCategory == nullptr ||
                        OtherCategory == TransformCategory ||
                        SurfaceCategories.Contains(OtherCategory))
                    {
                        continue;
                    }

                    const int32 ExistingOrder =
                        OtherCategory->GetSortOrder();

                    if (ExistingOrder > TransformOrder &&
                        ExistingOrder <= MAX_int32 - InsertCount)
                    {
                        OtherCategory->SetSortOrder(
                            ExistingOrder + InsertCount);
                    }
                }

                for (int32 Index = 0; Index < SurfaceCategories.Num(); ++Index)
                {
                    SurfaceCategories[Index]->SetSortOrder(
                        TransformOrder + Index + 1);
                }
            });
    }

    void AddNotice(
        IDetailCategoryBuilder& Category,
        const FText& Message)
    {
        Category.AddCustomRow(Message)
            .WholeRowContent()
            [
                SNew(STextBlock)
                    .Text(Message)
                    .Font(IDetailLayoutBuilder::GetDetailFont())
                    .AutoWrapText(true)
            ];
    }

    void AddSelectActorSection(
        IDetailLayoutBuilder& DetailBuilder,
        const TArray<TWeakObjectPtr<UActorComponent>>& Components)
    {
        TArray<TWeakObjectPtr<AActor>> Owners;

        for (const TWeakObjectPtr<UActorComponent>& WeakComponent : Components)
        {
            UActorComponent* Component = WeakComponent.Get();

            if (!IsValid(Component) ||
                Component->IsBeingDestroyed() ||
                Component->IsTemplate())
            {
                return;
            }

            AActor* Owner = Component->GetOwner();

            if (!IsValid(Owner) ||
                Owner->IsActorBeingDestroyed() ||
                Owner->IsTemplate())
            {
                return;
            }

            const UWorld* World = Owner->GetWorld();

            if (World == nullptr ||
                World->WorldType != EWorldType::Editor)
            {
                return;
            }

            Owners.AddUnique(TWeakObjectPtr<AActor>(Owner));
        }

        if (Owners.IsEmpty())
        {
            return;
        }

        AActor* FirstOwner = Owners[0].Get();

        if (!IsValid(FirstOwner))
        {
            return;
        }

        FString ClassName = FirstOwner->GetClass()->GetName();
        ClassName.RemoveFromEnd(TEXT("_C"));

        FText ActorDescription;

        if (Owners.Num() == 1)
        {
            ActorDescription = FText::Format(
                LOCTEXT(
                    "SelectedActorDescription",
                    "{0} - {1}"),
                FText::FromString(FirstOwner->GetActorLabel()),
                FText::FromString(ClassName));
        }
        else
        {
            bool bSameClass = true;

            for (const TWeakObjectPtr<AActor>& WeakOwner : Owners)
            {
                const AActor* Owner = WeakOwner.Get();

                if (Owner == nullptr ||
                    Owner->GetClass() != FirstOwner->GetClass())
                {
                    bSameClass = false;
                    break;
                }
            }

            ActorDescription = FText::Format(
                LOCTEXT(
                    "SelectedActorsDescription",
                    "{0} Actors - {1}"),
                FText::AsNumber(Owners.Num()),
                bSameClass
                ? FText::FromString(ClassName)
                : LOCTEXT(
                    "MultipleActorClasses",
                    "Multiple Classes"));
        }

        IDetailCategoryBuilder& Category =
            DetailBuilder.EditCategory(
                TEXT("Surface_Selection"),
                LOCTEXT("ComponentSurfaceCategory", "Surface"),
                ECategoryPriority::Default);

        Category.SetSortOrder(0);
        Category.InitiallyCollapsed(false);
        Category.RestoreExpansionState(false);

        PositionSurfaceCategories(DetailBuilder);

        const TWeakPtr<IPropertyUtilities> WeakUtilities =
            DetailBuilder.GetPropertyUtilities();

        Category.AddCustomRow(ActorDescription)
            .NameContent()
            [
                SNew(STextBlock)
                    .Text(ActorDescription)
                    .ToolTipText(ActorDescription)
                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
            .ValueContent()
            [
                SNew(SButton)
                    .Text(Owners.Num() == 1
                        ? LOCTEXT("SelectActor", "Select Actor")
                        : LOCTEXT("SelectActors", "Select Actors"))
                    .ToolTipText(LOCTEXT(
                        "SelectActorTooltip",
                        "Select the owning Actor to return to its Details panel."))
                    .OnClicked_Lambda(
                        [Components, WeakUtilities]()
                        {
                            QueueSurfaceSelection(
                                Components,
                                WeakUtilities,
                                true);

                            return FReply::Handled();
                        })
            ];
    }

    void AddSurfaceFavoriteRows(
        IDetailCategoryBuilder& FavoritesCategory,
        const Surface::FComponentGroup& ComponentGroup,
        const TArray<TSharedRef<IPropertyHandle>>& Properties)
    {
        if (Properties.IsEmpty())
        {
            return;
        }

        const FName GroupName(
            *(FString(TEXT("SurfaceFavoriteGroup|")) +
                ComponentGroup.StableId));

        IDetailGroup& FavoriteGroup = FavoritesCategory.AddGroup(
            GroupName,
            ComponentGroup.Label,
            false,
            true);

        FavoriteGroup.HeaderRow().NameContent()
            [
                SNew(STextBlock)
                    .Text(ComponentGroup.Label)
                    .ToolTipText(FText::Format(
                        LOCTEXT("FavoriteGroupTip", "{0}\nFavorite properties for this component. The favorite choice applies to its component class."),
                        ComponentGroup.Label))
                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ];

        for (const TSharedRef<IPropertyHandle>& Handle : Properties)
        {
            // A normal property row retains Unreal's editing, reset,
            // undo and favorite/unfavorite behavior.
            FavoriteGroup.AddPropertyRow(Handle);
        }
    }
}

FSurfaceCustomization::FSurfaceCustomization(
    const TSharedPtr<IDetailCustomization>& InOriginalCustomization)
    : OriginalCustomization(InOriginalCustomization)
{}

void FSurfaceCustomization::CustomizeDetails(
    IDetailLayoutBuilder& DetailBuilder)
{
    if (OriginalCustomization.IsValid())
    {
        OriginalCustomization->CustomizeDetails(DetailBuilder);
    }

    AddComponentDetails(DetailBuilder);
}

void FSurfaceCustomization::CustomizeDetails(
    const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder)
{
    if (!DetailBuilder.IsValid())
    {
        return;
    }

    if (OriginalCustomization.IsValid())
    {
        OriginalCustomization->CustomizeDetails(DetailBuilder);
    }

    AddComponentDetails(*DetailBuilder);
}

void FSurfaceCustomization::PendingDelete()
{
    if (OriginalCustomization.IsValid())
    {
        OriginalCustomization->PendingDelete();
    }
}

void FSurfaceCustomization::AddComponentDetails(
    IDetailLayoutBuilder& DetailBuilder)
{
    // Embedded component layouts participate only in favorite collection.
    if (RegisterComponentFavoriteCapture(DetailBuilder))
    {
        return;
    }

    if (GBuildingComponentDetails)
    {
        return;
    }

    GBuildingComponentDetails = true;
    ON_SCOPE_EXIT
    {
        GBuildingComponentDetails = false;
    };

    TArray<TWeakObjectPtr<UObject>> InspectedObjects;
    DetailBuilder.GetObjectsBeingCustomized(InspectedObjects);

    const TArray<TWeakObjectPtr<UObject>>& RootObjects =
        DetailBuilder.GetSelectedObjects();

    if (InspectedObjects.IsEmpty() ||
        InspectedObjects.Num() != RootObjects.Num())
    {
        return;
    }

    TSet<UObject*> RootSet;

    for (const TWeakObjectPtr<UObject>& Object : RootObjects)
    {
        RootSet.Add(Object.Get());
    }

    TSet<UObject*> UniqueObjects;

    for (const TWeakObjectPtr<UObject>& WeakObject : InspectedObjects)
    {
        UObject* Object = WeakObject.Get();

        if (!IsValid(Object) ||
            Object->IsTemplate() ||
            !RootSet.Contains(Object) ||
            UniqueObjects.Contains(Object))
        {
            return;
        }

        UniqueObjects.Add(Object);
    }

    // Directly selected components get the Select Actor section.
    if (Cast<UActorComponent>(InspectedObjects[0].Get()) != nullptr)
    {
        TArray<TWeakObjectPtr<UActorComponent>> Components;

        for (const TWeakObjectPtr<UObject>& Object : InspectedObjects)
        {
            UActorComponent* Component = Cast<UActorComponent>(Object.Get());

            if (Component == nullptr)
            {
                return;
            }

            Components.Add(TWeakObjectPtr<UActorComponent>(Component));
        }

        AddSelectActorSection(DetailBuilder, Components);
        return;
    }

    TArray<AActor*> Actors;

    for (const TWeakObjectPtr<UObject>& Object : InspectedObjects)
    {
        AActor* Actor = Cast<AActor>(Object.Get());

        if (Actor == nullptr || Actor->IsActorBeingDestroyed())
        {
            return;
        }

        const UWorld* World = Actor->GetWorld();

        if (World == nullptr ||
            World->WorldType != EWorldType::Editor)
        {
            return;
        }

        Actors.Add(Actor);
    }

    // A separate category gives Favorites its own expansion state.
    IDetailCategoryBuilder& FavoritesCategory =
        DetailBuilder.EditCategory(
            TEXT("Surface_Favorites"),
            LOCTEXT("SurfaceFavoritesCategory", "Surface - Favorites"),
            ECategoryPriority::Default);

    FavoritesCategory.SetSortOrder(MAX_int32 - 1);

    IDetailCategoryBuilder& Category =
        DetailBuilder.EditCategory(
            TEXT("Surface_Components"),
            LOCTEXT("Category", "Surface - Component Details"),
            ECategoryPriority::Default);

    Category.SetSortOrder(MAX_int32);

    PositionSurfaceCategories(DetailBuilder);

    const TSharedRef<SurfacePanel::FState> PanelState =
        SurfacePanel::GetState(DetailBuilder, Actors);

    SurfacePanel::ConfigureCategory(FavoritesCategory, PanelState, true);
    SurfacePanel::ConfigureCategory(Category, PanelState, false);

    const TWeakPtr<IPropertyUtilities> WeakUtilities =
        DetailBuilder.GetPropertyUtilities();

    // Preserve filters and expansion choices while refreshing the item list.
    PanelState->Items.Reset();

    SurfacePanel::AddControls(
        Category,
        PanelState,
        WeakUtilities);

    TArray<Surface::FComponentGroup> Groups;
    int32 UnmatchedCount = 0;

    if (!Surface::BuildComponentGroups(
        Actors,
        Groups,
        UnmatchedCount))
    {
        const FText Message = LOCTEXT(
            "UnsupportedSelection",
            "Select instances of the same Actor class to edit matching "
            "components together.");

        AddNotice(Category, Message);
        AddNotice(FavoritesCategory, Message);
        return;
    }

    if (UnmatchedCount > 0)
    {
        const FText Message = FText::Format(
            LOCTEXT(
                "Unmatched",
                "{0} component groups are omitted because they do not "
                "match across every selected Actor. Select one Actor "
                "to edit them."),
            FText::AsNumber(UnmatchedCount));

        AddNotice(Category, Message);
        AddNotice(FavoritesCategory, Message);
    }

    if (Groups.IsEmpty())
    {
        AddNotice(
            Category,
            LOCTEXT(
                "NoGroups",
                "No matching owned components are available for this selection."));
    }

    const bool bHideConstructionScriptComponents =
        GetDefault<UBlueprintEditorSettings>()->bHideConstructionScriptComponentsInDetailsView;

    int32 FavoriteCount = 0;
    int32 CaptureFailureCount = 0;

    for (const Surface::FComponentGroup& Group : Groups)
    {
        if (Group.Components.Num() != Actors.Num())
        {
            continue;
        }

        TArray<UObject*> ExternalObjects;
        ExternalObjects.Reserve(Group.Components.Num());

        bool bValidGroup = true;

        for (const TWeakObjectPtr<UActorComponent>& WeakComponent : Group.Components)
        {
            UActorComponent* Component = WeakComponent.Get();

            if (!IsValid(Component) ||
                Component->IsBeingDestroyed() ||
                Component->IsTemplate())
            {
                bValidGroup = false;
                break;
            }

            ExternalObjects.Add(Component);
        }

        if (!bValidGroup)
        {
            continue;
        }

        const FString ComponentId = Group.StableId;
        const FText ComponentLabel = Group.Label;

        const TArray<TWeakObjectPtr<UActorComponent>> ComponentsToSelect =
            Group.Components;

        SurfacePanel::FComponentItem FilterItem;
        FilterItem.Id = ComponentId;
        FilterItem.Label = ComponentLabel;
        for (const TWeakObjectPtr<UActorComponent>& WeakComponent : Group.Components)
        {
            if (UActorComponent* Component = WeakComponent.Get())
            {
                const FText Reason = GetDefaultHiddenReason(
                    *Component, bHideConstructionScriptComponents);
                if (!Reason.IsEmpty())
                {
                    // For multi-selection, default-hide when any matched component
                    // would be excluded from its Actor's standard component tree.
                    FilterItem.DefaultHiddenReason = Reason;
                    break;
                }
            }
        }
        PanelState->AddItem(FilterItem);

        Category.AddCustomRow(ComponentLabel)
            .Visibility(
                TAttribute<EVisibility>::CreateLambda(
                    [PanelState, ComponentId, ComponentLabel]()
                    {
                        return PanelState->PassesFilter(
                            ComponentId,
                            ComponentLabel)
                            ? EVisibility::Visible
                            : EVisibility::Collapsed;
                    }))
            .NameContent()
            [
                SNew(SButton)
                    .ButtonStyle(FAppStyle::Get(), "NoBorder")
                    .ContentPadding(FMargin(0.0f, 2.0f))
                    .HAlign(HAlign_Left)
                    .ToolTipText_Lambda(
                        [PanelState, ComponentId]()
                        {
                            return PanelState->ExpandedComponents.Contains(ComponentId)
                                ? LOCTEXT(
                                    "CollapseComponentDetails",
                                    "Collapse this component's details.")
                                : LOCTEXT(
                                    "ExpandComponentDetails",
                                    "Expand this component's details.");
                        })
                    .OnClicked_Lambda(
                        [PanelState, ComponentId]()
                        {
                            PanelState->ToggleExpanded(ComponentId);

                            return FReply::Handled();
                        })
                    [
                        SNew(SHorizontalBox)

                            + SHorizontalBox::Slot()
                            .AutoWidth()
                            .VAlign(VAlign_Center)
                            .Padding(0.0f, 0.0f, 6.0f, 0.0f)
                            [
                                SNew(SBox)
                                    .WidthOverride(16.0f)
                                    .HeightOverride(16.0f)
                                    .HAlign(HAlign_Center)
                                    .VAlign(VAlign_Center)
                                    [
                                        SNew(SImage)
                                            .Image_Lambda(
                                                [PanelState, ComponentId]()
                                                {
                                                    return FAppStyle::GetBrush(
                                                        PanelState->ExpandedComponents.Contains(
                                                            ComponentId)
                                                        ? "TreeArrow_Expanded"
                                                        : "TreeArrow_Collapsed");
                                                })
                                    ]
                            ]

                        + SHorizontalBox::Slot()
                            .FillWidth(1.0f)
                            .VAlign(VAlign_Center)
                            [
                                SNew(STextBlock)
                                    .Text(ComponentLabel)
                                    .ToolTipText(ComponentLabel)
                                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
                            ]
                    ]
            ]
        .ValueContent()
            [
                SNew(SButton)
                    .Text(ComponentsToSelect.Num() == 1
                        ? LOCTEXT("SelectComponent", "Select Component")
                        : LOCTEXT("SelectComponents", "Select Components"))
                    .ToolTipText(LOCTEXT(
                        "SelectComponentTooltip",
                        "Select this component in the editor to use its normal "
                        "Details and viewport tools."))
                    .OnClicked_Lambda(
                        [ComponentsToSelect, WeakUtilities]()
                        {
                            QueueSurfaceSelection(
                                ComponentsToSelect,
                                WeakUtilities,
                                false);

                            return FReply::Handled();
                        })
            ];

        FAddPropertyParams Params;
        Params
            .UniqueId(FName(
                *(FString(TEXT("Surface|")) + ComponentId)))
            .AllowChildren(true)
            .CreateCategoryNodes(true)
            .HideRootObjectNode(true);

        const TSharedRef<FSurfaceFavoriteCapture> Capture =
            MakeShared<FSurfaceFavoriteCapture>(ExternalObjects);

        IDetailPropertyRow* PropertyRow = nullptr;

        {
            const TSharedPtr<FSurfaceFavoriteCapture> PreviousCapture =
                GFavoriteCapture;

            GFavoriteCapture = Capture;

            ON_SCOPE_EXIT
            {
                GFavoriteCapture = PreviousCapture;
            };

            // The component layout, including its sort callbacks, is generated
            // synchronously while this external row is constructed.
            PropertyRow = Category.AddExternalObjects(
                ExternalObjects,
                EPropertyLocation::Default,
                Params);
        }

        if (PropertyRow == nullptr)
        {
            ++CaptureFailureCount;
            continue;
        }

        PropertyRow->Visibility(
            TAttribute<EVisibility>::CreateLambda(
                [PanelState, ComponentId, ComponentLabel]()
                {
                    const bool bPassesFilter =
                        PanelState->PassesFilter(
                            ComponentId,
                            ComponentLabel);

                    const bool bExpanded =
                        PanelState->ExpandedComponents.Contains(ComponentId);

                    return bPassesFilter && bExpanded
                        ? EVisibility::Visible
                        : EVisibility::Collapsed;
                }));

        if (!Capture->bCollected)
        {
            ++CaptureFailureCount;

            UE_LOG(
                LogSurfaceFavorites,
                Warning,
                TEXT("Favorite collection did not complete for '%s'. ")
                TEXT("Component callback registered: %s."),
                *ComponentLabel.ToString(),
                Capture->bCallbackRegistered ? TEXT("yes") : TEXT("no"));

            continue;
        }

        UE_LOG(
            LogSurfaceFavorites,
            Verbose,
            TEXT("Collected %d favorite property handles for '%s'."),
            Capture->Properties.Num(),
            *ComponentLabel.ToString());

        // These rows deliberately have no Surface filter or expansion binding.
        // They remain available when the main component section is hidden.
        AddSurfaceFavoriteRows(
            FavoritesCategory,
            Group,
            Capture->Properties);

        FavoriteCount += Capture->Properties.Num();
    }

    // Record defaults once after discovering the complete list, not once per component.
    PanelState->Save();

    if (FavoriteCount == 0)
    {
        AddNotice(
            FavoritesCategory,
            LOCTEXT(
                "NoSurfaceFavorites",
                "Right-click a component property in Surface and add it "
                "to Favorites. Favorites apply to that component class, so "
                "matching components appear here in separate groups."));
    }

    if (CaptureFailureCount > 0)
    {
        AddNotice(
            FavoritesCategory,
            LOCTEXT(
                "SurfaceFavoriteCollectionIncomplete",
                "Some component favorites could not be collected. "
                "See LogSurfaceFavorites in the Output Log."));
    }
}

#undef LOCTEXT_NAMESPACE