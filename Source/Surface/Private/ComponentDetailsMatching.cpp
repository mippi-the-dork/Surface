// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComponentDetailsMatching.h"

#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "GameFramework/Actor.h"
#include "UObject/Class.h"
#include "UObject/UObjectGlobals.h"

namespace Surface
{
    namespace Private
    {
        // Object names cannot contain this delimiter. The class path distinguishes
        // Blueprint classes with identical short names in different packages.
        FString MakeKey(const UActorComponent& Component, const AActor& Owner)
        {
            return Component.GetClass()->GetPathName() + TEXT("|") +
                Component.GetPathName(&Owner) + TEXT("|") +
                FString::FromInt(static_cast<int32>(Component.CreationMethod));
        }
    }

    bool BuildComponentGroups(
        const TArray<AActor*>& Actors,
        TArray<FComponentGroup>& OutGroups,
        int32& OutUnmatchedGroupCount)
    {
        OutGroups.Reset();
        OutUnmatchedGroupCount = 0;
        if (Actors.IsEmpty() || !IsValid(Actors[0]))
        {
            return false;
        }

        UClass* ActorClass = Actors[0]->GetClass();
        TSet<AActor*> SeenActors;
        TMap<FString, FComponentGroup> Groups;

        for (AActor* Actor : Actors)
        {
            if (!IsValid(Actor) || Actor->IsActorBeingDestroyed() ||
                Actor->GetClass() != ActorClass || SeenActors.Contains(Actor))
            {
                return false;
            }
            SeenActors.Add(Actor);

            TArray<UActorComponent*> Components;
            // Includes non-scene components. Does not descend into child Actors.
            Actor->GetComponents<UActorComponent>(Components, false);
            TSet<FString> SeenKeys;
            for (UActorComponent* Component : Components)
            {
                if (!IsValid(Component) || Component->IsBeingDestroyed() ||
                    Component->IsTemplate() || Component->GetOwner() != Actor)
                {
                    continue;
                }

                const FString Key = Private::MakeKey(*Component, *Actor);
                // Defensive uniqueness check even though valid object paths are
                // unique within an Actor. A duplicate must never count twice.
                if (SeenKeys.Contains(Key))
                {
                    return false;
                }
                SeenKeys.Add(Key);

                FComponentGroup& Group = Groups.FindOrAdd(Key);
                if (Group.Components.IsEmpty())
                {
                    Group.StableId = Key;
                    Group.Label = FText::FromString(Component->GetName() + TEXT(" - ") +
                        Component->GetClass()->GetName());
                }
                Group.Components.Add(Component);
            }
        }

        for (const TPair<FString, FComponentGroup>& Pair : Groups)
        {
            if (Pair.Value.Components.Num() == Actors.Num())
            {
                OutGroups.Add(Pair.Value);
            }
            else
            {
                ++OutUnmatchedGroupCount;
            }
        }

        OutGroups.Sort([](const FComponentGroup& Left, const FComponentGroup& Right)
        {
            const int32 LabelOrder = Left.Label.ToString().Compare(Right.Label.ToString());
            return LabelOrder == 0 ? Left.StableId < Right.StableId : LabelOrder < 0;
        });
        return true;
    }
}
