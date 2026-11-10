// Copyright Mippithedork 2026, Inc. All Rights Reserved.
#include "SurfaceMovementPolicy.h"
#include "SurfaceMovementSettings.h"
#include "SurfaceScrollAnchor.h"
#include "IDetailsView.h"
#include "Widgets/Views/STableViewBase.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurfaceScrollAnchor, Log, All);
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "IPropertyUtilities.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EditorMetadataOverrides.h"

namespace SurfaceMovementPolicy
{
    namespace
    {
        // Only weak references: the Details view and actor must not be kept alive by Surface.
        TSet<TWeakObjectPtr<AActor>> MovingActors;
        uint64 FavoritesRefreshRevision = 0;
        TMap<TWeakObjectPtr<AActor>, TWeakPtr<IPropertyUtilities>> DetailsUtilities;
        TMap<TWeakObjectPtr<AActor>, TWeakPtr<IDetailsView>> DetailsViews;
        struct FScrollSession
        {
            TWeakPtr<IDetailsView> View;
            TWeakPtr<STableViewBase> Table;
            SurfaceScrollAnchor::FRowAnchor Anchor;
            SurfaceScrollAnchor::FRestoreStability RestoreState;
            bool bDuringMovement = true;
            bool bNeedsSurfaceGeometry = false;
            float LastAppliedOffset = -1.0f;
        };
        TMap<TWeakObjectPtr<AActor>, FScrollSession> ScrollSessions;
        float ScrollCheckAccumulation = 0.0f;

        bool IsObservedSelection(const TSharedPtr<IDetailsView>& View, AActor* Actor)
        {
            if (!View.IsValid() || !IsValid(Actor)) { return false; }
            for (const TWeakObjectPtr<AActor>& Selected : View->GetSelectedActors())
            {
                if (Selected.Get() == Actor) { return true; }
            }
            return false;
        }

        void CaptureScroll(AActor* Actor)
        {
            if (!Actor || ScrollSessions.Contains(Actor)) { return; }
            const TWeakPtr<IDetailsView>* WeakView = DetailsViews.Find(Actor);
            TSharedPtr<IDetailsView> View = WeakView ? WeakView->Pin() : nullptr;
            if (!IsObservedSelection(View, Actor)) { return; }
            TSharedPtr<STableViewBase> Table = SurfaceScrollAnchor::FindDetailsTable(View).Pin();
            if (!Table.IsValid())
            {
                UE_LOG(LogSurfaceScrollAnchor, Verbose, TEXT("Scroll anchor unavailable: native Details table not found."));
                return;
            }
            const TArray<SurfaceScrollAnchor::FNamedRow> Rows = SurfaceScrollAnchor::GetNamedRows(View);
            const float OriginalScroll = Table->GetScrollOffset();
            const SurfaceScrollAnchor::FRowAnchor Anchor = SurfaceScrollAnchor::CaptureRow(
                Rows, OriginalScroll);
            // Actor properties are not necessarily present in the visible
            // region when the user is scrolled into Surface's external rows.
            // In that case prefer measured space over discarding the session.
            bool bNearAnchor = false;
            for (const SurfaceScrollAnchor::FNamedRow& Row : Rows)
            {
                if (Row.Path == Anchor.PropertyPath)
                {
                    bNearAnchor = Row.Index <= OriginalScroll + 3.0f;
                    break;
                }
            }
            FScrollSession& Session = ScrollSessions.Add(Actor);
            Session.View = View;
            Session.Table = Table;
            Session.Anchor = Anchor;
            Session.bNeedsSurfaceGeometry = !bNearAnchor;
            UE_LOG(LogSurfaceScrollAnchor, Verbose,
                TEXT("Captured scroll anchor %s (row offset %.2f, measured geometry fallback %s)"),
                *Anchor.PropertyPath, Anchor.OffsetWithinFirstRow,
                Session.bNeedsSurfaceGeometry ? TEXT("on") : TEXT("off"));
        }
        // Metadata is a list of paths, not a live property handle. A class-wide
        // snapshot detects favorites changed in other Details views.
        TMap<TWeakObjectPtr<UClass>, TArray<FString>> FavoritesMetadata;

        bool ReadFavorites(UClass* Class, TArray<FString>& OutPaths)
        {
            OutPaths.Reset();
            if (!Class || !GEditor) { return false; }
            UEditorMetadataOverrides* Overrides = GEditor->GetEditorSubsystem<UEditorMetadataOverrides>();
            if (!Overrides) { return false; }
            Overrides->GetArrayMetadata(Class, FName(TEXT("FavoriteProperties")), OutPaths);
            OutPaths.Sort();
            return true;
        }

        void ObserveClasses(AActor* Actor)
        {
            if (!IsValid(Actor)) { return; }
            TInlineComponentArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (!IsValid(Component)) { continue; }
                UClass* Class = Component->GetClass();
                if (!FavoritesMetadata.Contains(Class))
                {
                    TArray<FString> Paths;
                    if (ReadFavorites(Class, Paths))
                    {
                        FavoritesMetadata.Add(Class, MoveTemp(Paths));
                    }
                }
            }
        }

        AActor* GetActor(UObject& Object)
        {
            if (AActor* Actor = Cast<AActor>(&Object)) { return Actor; }
            if (UActorComponent* Component = Cast<UActorComponent>(&Object)) { return Component->GetOwner(); }
            return nullptr;
        }
        void Refresh(AActor* Actor)
        {
            if (!Actor) { return; }
            if (const TWeakPtr<IPropertyUtilities>* Weak = DetailsUtilities.Find(Actor))
            {
                if (TSharedPtr<IPropertyUtilities> Utilities = Weak->Pin())
                {
                    // Request, rather than synchronous force, avoids rebuilding inside editor delegates.
                    Utilities->RequestForceRefresh();
                }
            }
        }
    }

    bool ShouldSkipExternalLayout(bool bIsSelectedActorMoving, bool bOptimizationEnabled)
    {
        return bIsSelectedActorMoving && bOptimizationEnabled;
    }
    uint64 GetFavoritesRefreshRevision() { return FavoritesRefreshRevision; }
    bool ShouldPreserveSpaceForSelection(const TArray<AActor*>& Actors)
    {
        for (AActor* Actor : Actors)
        {
            if (Actor && MovingActors.Contains(Actor))
            {
                if (const FScrollSession* Session = ScrollSessions.Find(Actor))
                {
                    if (Session->bNeedsSurfaceGeometry) { return true; }
                }
            }
        }
        return false;
    }
    bool IsLightweightForSelection(const TArray<AActor*>& Actors)
    {
        const bool bEnabled = GetDefault<USurfaceMovementSettings>()->bLightweightDuringMovement;
        if (!bEnabled) { return false; }
        for (AActor* Actor : Actors)
        {
            if (Actor && MovingActors.Contains(Actor)) { return true; }
        }
        return false;
    }
    void ObserveDetails(const TArray<AActor*>& Actors, const TWeakPtr<IPropertyUtilities>& Utilities)
    {
        if (!Utilities.IsValid()) { return; }
        for (AActor* Actor : Actors)
        {
            if (IsValid(Actor)) { DetailsUtilities.Add(Actor, Utilities); ObserveClasses(Actor); }
        }
        // Bound memory growth for long sessions with many different selections.
        for (auto It = DetailsUtilities.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); }
        }
    }
    void ObserveView(const TArray<AActor*>& Actors, const TWeakPtr<IDetailsView>& View)
    {
        if (!View.IsValid()) { return; }
        for (AActor* Actor : Actors)
        {
            if (IsValid(Actor)) { DetailsViews.Add(Actor, View); }
        }
        for (auto It = DetailsViews.CreateIterator(); It; ++It)
        {
            if (!It.Key().IsValid() || !It.Value().IsValid()) { It.RemoveCurrent(); }
        }
    }

    bool RestoreScrollAnchors(float DeltaTime)
    {
        // Details' STreeView is virtualized, so defer until after its rows
        // have been rebuilt. Avoid expensive row discovery every Slate tick.
        if (ScrollSessions.IsEmpty()) { return true; }
        ScrollCheckAccumulation += DeltaTime;
        if (ScrollCheckAccumulation < 0.05f) { return true; }
        ScrollCheckAccumulation = 0.0f;
        for (auto It = ScrollSessions.CreateIterator(); It; ++It)
        {
            AActor* Actor = It.Key().Get();
            FScrollSession& Session = It.Value();
            TSharedPtr<IDetailsView> View = Session.View.Pin();
            TSharedPtr<STableViewBase> Table = Session.Table.Pin();
            if (!IsObservedSelection(View, Actor) || !Table.IsValid())
            {
                It.RemoveCurrent();
                continue;
            }
            const TArray<SurfaceScrollAnchor::FNamedRow> Rows = SurfaceScrollAnchor::GetNamedRows(View);
            const TOptional<float> Target = SurfaceScrollAnchor::ResolveOffset(Session.Anchor, Rows);
            // Hash the complete actor row mapping, not just its count: changes
            // elsewhere in the Details layout invalidate a previously stable
            // scroll position.
            uint32 LayoutHash = GetTypeHash(Rows.Num());
            for (const SurfaceScrollAnchor::FNamedRow& Row : Rows)
            {
                LayoutHash = HashCombineFast(LayoutHash, HashCombineFast(GetTypeHash(Row.Index), GetTypeHash(Row.Path)));
            }
            if (Session.bDuringMovement)
            {
                if (Target.IsSet() && FMath::Abs(Table->GetScrollOffset() - Target.GetValue()) > 0.1f)
                {
                    Table->SetScrollOffset(Target.GetValue());
                    Session.LastAppliedOffset = Target.GetValue();
                }
                continue;
            }

            Session.RestoreState.Observe(LayoutHash, Target.IsSet(), 0.05f);
            if (Target.IsSet())
            {
                const float CurrentOffset = Table->GetScrollOffset();
                if (FMath::Abs(CurrentOffset - Target.GetValue()) > 0.1f)
                {
                    // Once the native layout has already stabilized, avoid
                    // fighting a user's subsequent manual scroll gesture.
                    if (Session.RestoreState.UnchangedSeconds > 0.45f &&
                        Session.RestoreState.SuccessfulRestores > 0 &&
                        Session.LastAppliedOffset >= 0.0f &&
                        FMath::Abs(CurrentOffset - Session.LastAppliedOffset) > 0.5f)
                    {
                        It.RemoveCurrent();
                        continue;
                    }
                    Table->SetScrollOffset(Target.GetValue());
                    Session.LastAppliedOffset = Target.GetValue();
                }
                ++Session.RestoreState.SuccessfulRestores;
            }
            if (Session.RestoreState.IsSettled() || Session.RestoreState.HasTimedOut())
            {
                It.RemoveCurrent();
            }
        }
        return true;
    }

    void BeginObjectMovement(UObject& Object)
    {
        AActor* Actor = GetActor(Object);
        if (!IsValid(Actor)) { return; }
        if (!MovingActors.Contains(Actor))
        {
            CaptureScroll(Actor);
            MovingActors.Add(Actor);
            if (GetDefault<USurfaceMovementSettings>()->bLightweightDuringMovement) { Refresh(Actor); }
        }
    }
    void EndObjectMovement(UObject& Object)
    {
        AActor* Actor = GetActor(Object);
        if (!Actor) { return; }
        if (MovingActors.Remove(Actor) > 0)
        {
            // Invalidate session-only negative Favorites information after reconstruction.
            ++FavoritesRefreshRevision;
            if (FScrollSession* Session = ScrollSessions.Find(Actor))
            {
                Session->bDuringMovement = false;
                Session->RestoreState = SurfaceScrollAnchor::FRestoreStability();
            }
            // Rebuild with new UObject instances, never reuse pre-reconstruction handles.
            Refresh(Actor);
        }
    }
    bool PollFavoritesMetadata(float DeltaTime)
    {
        // The poll is intentionally cheap: one metadata read per observed class,
        // without asking PropertyEditor to build or walk a single property row.
        TSet<UClass*> ChangedClasses;
        for (auto It = FavoritesMetadata.CreateIterator(); It; ++It)
        {
            UClass* Class = It.Key().Get();
            if (!IsValid(Class)) { It.RemoveCurrent(); continue; }
            TArray<FString> Current;
            if (ReadFavorites(Class, Current) && Current != It.Value())
            {
                It.Value() = MoveTemp(Current);
                ChangedClasses.Add(Class);
            }
        }
        if (ChangedClasses.IsEmpty()) { return true; }
        ++FavoritesRefreshRevision;

        for (auto It = DetailsUtilities.CreateIterator(); It; ++It)
        {
            AActor* Actor = It.Key().Get();
            if (!IsValid(Actor) || !It.Value().IsValid())
            {
                It.RemoveCurrent();
                continue;
            }
            // Never rebuild Surface's external rows in the middle of a drag.
            // EndObjectMovement already schedules a safe refresh afterward.
            if (MovingActors.Contains(Actor)) { continue; }
            TInlineComponentArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (IsValid(Component) && ChangedClasses.Contains(Component->GetClass()))
                {
                    Refresh(Actor);
                    break;
                }
            }
        }
        return true;
    }
    void Reset()
    {
        MovingActors.Reset();
        DetailsUtilities.Reset();
        DetailsViews.Reset();
        ScrollSessions.Reset();
        FavoritesMetadata.Reset();
    }
}
