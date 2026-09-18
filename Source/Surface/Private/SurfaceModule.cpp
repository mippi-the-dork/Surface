// Copyright Epic Games, Inc. All Rights Reserved.

#include "SurfaceModule.h"

#include "SurfaceCustomization.h"

#include "Components/ActorComponent.h"
#include "CoreGlobals.h"
#include "GameFramework/Actor.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"

DEFINE_LOG_CATEGORY_STATIC(LogSurface, Log, All);

void FSurfaceModule::StartupModule()
{
    if (!Registrations.IsEmpty() || IsRunningCommandlet())
    {
        return;
    }

    // Ensure stock customizations exist before capturing their registrations.
    FModuleManager::LoadModuleChecked<IModuleInterface>(
        TEXT("DetailCustomizations"));

    FPropertyEditorModule& PropertyEditor =
        FModuleManager::LoadModuleChecked<FPropertyEditorModule>(
            TEXT("PropertyEditor"));

    const FName ActorClassName = AActor::StaticClass()->GetFName();

    const FDetailLayoutCallback* ExistingActor =
        PropertyEditor.GetClassNameToDetailLayoutNameMap().Find(
            ActorClassName);

    if (ExistingActor == nullptr ||
        !ExistingActor->DetailLayoutDelegate.IsBound())
    {
        // Preserve the existing fail-closed behavior for Actor Details.
        UE_LOG(
            LogSurface,
            Error,
            TEXT("No existing Actor customization was found. ")
            TEXT("Surface was not registered."));
        return;
    }

    Registrations.Reserve(2);

    auto RegisterComposite =
        [this, &PropertyEditor](FName ClassName)
        {
            FLayoutRegistration& Registration =
                Registrations.AddDefaulted_GetRef();

            Registration.ClassName = ClassName;

            FOnGetDetailCustomizationInstance PreviousFactory;

            if (const FDetailLayoutCallback* Existing =
                PropertyEditor.GetClassNameToDetailLayoutNameMap().Find(
                    ClassName))
            {
                Registration.PreviousLayout = *Existing;
                PreviousFactory = Existing->DetailLayoutDelegate;
            }

            Registration.InstalledFactory =
                FOnGetDetailCustomizationInstance::CreateLambda(
                    [PreviousFactory]() -> TSharedRef<IDetailCustomization>
                    {
                        TSharedPtr<IDetailCustomization> Original;

                        if (PreviousFactory.IsBound())
                        {
                            Original = PreviousFactory.Execute();
                        }

                        return MakeShared<FSurfaceCustomization>(Original);
                    });

            FRegisterCustomClassLayoutParams Params;

            if (Registration.PreviousLayout.IsSet())
            {
                Params.OptionalOrder =
                    Registration.PreviousLayout->Order;
            }

            PropertyEditor.RegisterCustomClassLayout(
                ClassName,
                Registration.InstalledFactory,
                Params);
        };

    RegisterComposite(ActorClassName);
    RegisterComposite(UActorComponent::StaticClass()->GetFName());

    PropertyEditor.NotifyCustomizationModuleChanged();
}

void FSurfaceModule::ShutdownModule()
{
    // Never load modules during shutdown.
    FPropertyEditorModule* PropertyEditor =
        FModuleManager::GetModulePtr<FPropertyEditorModule>(
            TEXT("PropertyEditor"));

    bool bChangedRegistration = false;

    if (PropertyEditor != nullptr)
    {
        for (int32 Index = Registrations.Num() - 1; Index >= 0; --Index)
        {
            const FLayoutRegistration& Registration =
                Registrations[Index];

            const FDetailLayoutCallback* Current =
                PropertyEditor->GetClassNameToDetailLayoutNameMap().Find(
                    Registration.ClassName);

            // Another plugin may have replaced our registration.
            // Do not overwrite that plugin's callback.
            if (Current == nullptr ||
                Current->DetailLayoutDelegate.GetHandle() !=
                Registration.InstalledFactory.GetHandle())
            {
                continue;
            }

            PropertyEditor->UnregisterCustomClassLayout(
                Registration.ClassName);

            bChangedRegistration = true;

            if (!IsEngineExitRequested() &&
                Registration.PreviousLayout.IsSet() &&
                Registration.PreviousLayout->DetailLayoutDelegate.IsBound() &&
                FModuleManager::Get().IsModuleLoaded(
                    TEXT("DetailCustomizations")))
            {
                FRegisterCustomClassLayoutParams Params;
                Params.OptionalOrder =
                    Registration.PreviousLayout->Order;

                PropertyEditor->RegisterCustomClassLayout(
                    Registration.ClassName,
                    Registration.PreviousLayout->DetailLayoutDelegate,
                    Params);
            }
        }

        if (bChangedRegistration && !IsEngineExitRequested())
        {
            PropertyEditor->NotifyCustomizationModuleChanged();
        }
    }

    Registrations.Reset();
}

IMPLEMENT_MODULE(FSurfaceModule, Surface)