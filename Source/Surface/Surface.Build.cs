// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Surface : ModuleRules
{
    public Surface(ReadOnlyTargetRules Target) : base(Target)
    {
        // The property is DefaultBuildSettings, not BuildSettingsVersion.
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // This small module deliberately compiles without unity so missing direct
        // includes cannot be masked by an unrelated translation unit.
        bUseUnity = false;

        // There is no public C++ interface. All dependencies are private.
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "Slate",
            "SlateCore",
            "PropertyEditor",
            "DetailCustomizations",
            "UnrealEd",
            "BlueprintGraph"
        });
    }
}
