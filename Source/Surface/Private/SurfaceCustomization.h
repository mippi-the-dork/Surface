#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;

/** Preserves the existing Actor layout and appends owned component property rows. */
class FSurfaceCustomization final : public IDetailCustomization
{
public:
    explicit FSurfaceCustomization(
        const TSharedPtr<IDetailCustomization>& InOriginalCustomization);

    // Forward both entry points so existing customizations that require the
    // shared layout overload retain their weak-layout lifetime behavior.
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;
    virtual void CustomizeDetails(const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder) override;
    virtual void PendingDelete() override;

private:
    void AddComponentDetails(IDetailLayoutBuilder& DetailBuilder);

    TSharedPtr<IDetailCustomization> OriginalCustomization;
};
