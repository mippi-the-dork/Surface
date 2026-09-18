// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Internationalization/Text.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AActor;
class UActorComponent;

namespace Surface
{
    /** A complete match: exactly one component for each inspected Actor. */
    struct FComponentGroup
    {
        FString StableId;
        FText Label;
        TArray<TWeakObjectPtr<UActorComponent>> Components;
    };

    /**
     * Intersects components by actor-relative object path, exact component class,
     * and creation method. Does not pair components by array index or class alone.
     * Actors must be valid, unique and all of the same exact Actor class.
     * Returns false for invalid input, leaving no potentially partial edit groups.
     */
    bool BuildComponentGroups(
        const TArray<AActor*>& Actors,
        TArray<FComponentGroup>& OutGroups,
        int32& OutUnmatchedGroupCount);
}
