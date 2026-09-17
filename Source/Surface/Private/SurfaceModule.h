#pragma once

#include "Containers/Array.h"
#include "Misc/Optional.h"
#include "Modules/ModuleInterface.h"
#include "PropertyEditorDelegates.h"
#include "UObject/NameTypes.h"

/**
 * Preserves existing Actor and ActorComponent customizations while registering
 * Surface's additions.
 */
class FSurfaceModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    // Live Details views can retain customization instances and Slate callbacks.
    // Registration changes therefore require an editor restart.
    virtual bool SupportsDynamicReloading() override
    {
        return false;
    }

private:
    struct FLayoutRegistration
    {
        FName ClassName;
        TOptional<FDetailLayoutCallback> PreviousLayout;
        FOnGetDetailCustomizationInstance InstalledFactory;
    };

    TArray<FLayoutRegistration> Registrations;
};