# Surface

Surface is an Unreal Engine 5.8 Editor plugin for editing an Actor's component properties directly from the Actor's Details panel. It adds component filtering, a separate favorites section, and buttons for moving between Actor and component selection. No engine modifications or component tags are required.

## Install

1. Close Unreal Editor.
2. Place the `Surface` folder inside your project's `Plugins` folder. The descriptor must be at `YourProject/Plugins/Surface/Surface.uplugin`.
3. For an initial source installation into a C++ project, generate project files, open the solution in Visual Studio, and build **Development Editor / Win64** using your UE 5.8 installation.
4. Open the project. Find **Surface** under **Edit > Plugins**, enable it if needed, and restart when prompted. Manually editing the `.uproject` is normally unnecessary.
5. Select a placed Actor in the level. Select the Actor itself in the component hierarchy to see its Surface sections.

A source distribution requires a compatible C++ toolchain. For a Blueprint-only project, use a compatible packaged plugin or add a C++ class to establish a source-build workflow.

## Using Surface

### Surface - Component Details

This category has a magnifying-glass icon and contains a header for each available component, formatted as `ComponentName - ComponentClassName`, such as `PointLight - PointLightComponent`.

Expand a component header to edit its properties. Native property categories, arrays, structs, edit conditions, and supported property customizations remain available. Use **Select Component** beside the header when you need that component's normal Details panel or viewport tools.

When a component is selected directly, its **Surface** section displays the owning Actor's label and class, with a **Select Actor** button to return to the Actor's Details panel. Multi-selection uses the corresponding plural selection buttons.

When a recognized Transform category is present, Surface inserts **Surface - Favorites**, followed by **Surface - Component Details**, immediately after it. Existing categories retain their relative order. Without a recognized Transform category, the Actor's Surface sections use their fallback sort positions and may appear near the bottom.

### Component filters

| Control | Behavior |
| --- | --- |
| Search components | Filters by component name or class. It works together with the checkbox filter. |
| Components menu | Lists all available matched components, including entries currently hidden by search or unchecked. |
| Component checkbox | Checked shows that component when it also matches the search. Unchecked hides it from Component Details. |
| Check All | Checks every available component, including default-hidden entries. Keeps the search text. |
| Uncheck All | Unchecks every available component. |
| Clear Filters | Clears the search and shows every available component. Preserves expansion choices. Disabled when the search is empty and every current component is checked. |
| Refresh Components | Rebuilds the available component list while retaining saved filter and expansion choices for matching identities. |
| Showing X of Y | Counts components passing both filters, regardless of whether their headers are expanded. |

Refresh Components is useful after scripts or unusual Editor operations change component membership without triggering the normal Details refresh. It is not needed after every property edit.

#### Default-hidden components

For components Surface has not previously encountered for that Actor selection, initial checkbox states follow the stock UE 5.8 level-instance component-tree rules. These account for visualization helpers, the construction-script component visibility setting, nested default subobjects beneath construction-script components, and native components without an editable component property. The Actor's root component is included by default, matching the engine's exception.

These entries remain available in the Components menu, with tooltips explaining their default exclusion. Enabling one changes its visibility in Surface; it does not override Unreal's property editability rules. Billboard components and C++ components are not excluded solely because of their type or origin.

Previously saved visibility choices take precedence over defaults. **Clear Filters** shows everything; it does not restore the initial exclusions. Other plugins or specialized component trees can impose additional rules that Surface does not reproduce.

### Surface - Favorites

This separate category has a star icon and expands or collapses independently of Component Details.

Right-click a component property and use Unreal's favorite action. Surface collects the component favorites and displays them here, grouped by component. These rows remain available when a component is filtered out or its Component Details header is collapsed.

Favorites follow Unreal's class/property rules. For example, favoriting Light Color on one PointLightComponent can show Light Color for all three PointLightComponents on the Actor. Each row still edits its own component; favoriting a field does not link their values.

Surface does not insert these component properties into the Actor's built-in Favorites category. **Surface - Favorites** is the dedicated location for them. Unreal's favorites feature must be enabled for favorite collection to be available.

### Saved preferences

Surface stores search text, checkbox visibility, individual component-header expansion, and the two Surface categories' expansion choices in local per-project Editor preferences. These UI choices do not modify Actor assets or map data.

Preferences are scoped to an Actor selection. Selecting two Actors together has separate preferences from selecting either Actor alone. New selections start with Component Details collapsed, individual component headers collapsed, and Surface - Favorites expanded. Native nested property-category expansion remains managed by Unreal.

Persistence uses the level and Actor identity, plus component identity. Renaming or recreating components, moving Actors between levels, or renaming maps can produce a new identity and therefore new defaults. Auto-generated helper components are particularly susceptible to identity changes.

## Multi-selection

Select instances of the same exact Actor class, including multiple instances of one Blueprint class. Each displayed group contains exactly one corresponding component from every selected Actor.

Matching requires the same relative object path, exact component class, and creation method. Array order is not used as identity, and components are not merged merely because they share a type. Missing or differently named counterparts are omitted with a notice. Select an individual Actor to edit its unmatched components.

Independently added components and construction scripts that regenerate component names can prevent a match. For matching groups, native multiple-value indicators and editing apply to the intended components across the selection.

## Scope and limitations

- Surface targets placed Actor instances and their owned components in Editor worlds. Blueprint class defaults, templates, preview objects, and runtime PIE/simulation instances are outside Surface's scope.
- A ChildActorComponent is included, but Surface does not recursively inspect the separate Actor it spawns or other attached Actors.
- Existing Actor controls are retained. Surface adjusts category sort indices to insert its sections; it does not replace the Actor's normal Transform workflow.
- Specialized component tools and custom Details views are not guaranteed to appear in the embedded rows. Select Component provides access to the normal component workflow.
- A later plugin registration or a view-specific customization can override or bypass Surface's integration.
- **Component indentation:** Native component categories may appear less indented than their component header. These properties still belong to that component.
- Filters control displayed rows. Component layouts are still generated to support native editing and favorite collection, so filtering is not lazy loading and does not remove the initial generation cost.
- Property edits can dirty a level, trigger construction scripts, and be overwritten by construction-script-authored values, just as normal component edits can.

No Niagara module dependency is required merely to display an enabled Niagara component's reflected properties. Native property editing continues to handle transactions, notifications, undo, and supported reset-to-default behavior.

## Package for distribution

Run the included helper in PowerShell from the plugin folder:

```powershell
.\Scripts\PackagePlugin.ps1 -EngineRoot 'F:\Program Files\Epic Games\UE_5.8' -OutputDirectory 'H:\PluginBuilds\Surface-5.8'
```

Choose a new output folder outside the plugin source folder. The script refuses an existing destination and invokes Unreal's `BuildPlugin` for Win64 with strict include checking.

Distribute the resulting plugin folder with its Source and Binaries intact. Prebuilt binaries must match the recipient's engine build and platform; a 5.8 target does not guarantee compatibility across every patch or custom engine build.

Surface is an Editor-only module. Close Unreal before source builds; use a restart for registration and enable/disable changes.

## File structure

| Relative path | Purpose |
| --- | --- |
| `Surface.uplugin` | Plugin metadata and Editor-only module descriptor |
| `Source/Surface/Surface.Build.cs` | Build settings and dependencies, including UnrealEd and BlueprintGraph |
| `Source/Surface/Private/SurfaceModule.h` | Module registration and lifetime state |
| `Source/Surface/Private/SurfaceModule.cpp` | Details customization registration and cleanup |
| `Source/Surface/Private/SurfaceCustomization.h` | Composite customization interface |
| `Source/Surface/Private/SurfaceCustomization.cpp` | Component rows, favorites, selection buttons, sorting, and initial visibility rules |
| `Source/Surface/Private/SurfacePanelControls.h` | Filter controls, header icons, and saved UI preferences |
| `Source/Surface/Private/ComponentDetailsMatching.h` | Component group interface |
| `Source/Surface/Private/ComponentDetailsMatching.cpp` | Multi-selection matching |
| `Source/Surface/Private/ComponentDetailsMatchingTests.cpp` | Matching automation test |
| `Scripts/PackagePlugin.ps1` | Win64 packaging helper |
| `Docs/Sources.md` | API references and engine-source findings |

See [Sources](Docs/Sources.md) for API and implementation references.
