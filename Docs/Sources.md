# Surface sources and implementation notes

API and engine-source references for Surface's component details, filtering, favorites, selection controls, and saved preferences.

## Public API references

| Reference | Use in Surface |
| --- | --- |
| [Module properties](https://dev.epicgames.com/documentation/en-us/unreal-engine/module-properties-in-unreal-engine) | Module build settings and dependencies. Surface uses `DefaultBuildSettings = BuildSettingsVersion.V7`, `IncludeOrderVersion = EngineIncludeOrderVersion.Latest`, and explicit/shared PCH usage. |
| [FPropertyEditorModule](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/FPropertyEditorModule) | Customization registration, existing callback lookup, unregistration, and change notification. |
| [FDetailLayoutCallback](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/FDetailLayoutCallback) and [FRegisterCustomClassLayoutParams](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/FRegisterCustomClassLayoutParams) | Existing customization factories and registration ordering. |
| [IDetailCustomization](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IDetailCustomization) | Composite customization entry points and pending-delete lifecycle. |
| [IDetailLayoutBuilder](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IDetailLayoutBuilder) | Inspected/root object checks, property utilities, category creation, and sort callbacks. |
| [IDetailCategoryBuilder](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IDetailCategoryBuilder) | External-object rows, category ordering, custom icon/title headers, expansion callbacks, and reading default properties. |
| [FAddPropertyParams](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/FAddPropertyParams) | Stable external-row identifiers, native category generation, and hiding the external object's redundant root row. |
| [IDetailPropertyRow](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IDetailPropertyRow) | External row visibility and native property widgets. |
| [IPropertyHandle](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IPropertyHandle) | Favorite checks, outer-object validation, duplicate-node checks, and native property editing. |
| [IDetailGroup](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IDetailGroup) | Grouping collected favorite property rows by component. |
| [AActor::GetComponents](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/AActor/GetComponents) | Enumerating owned components without recursively traversing child Actors. |
| [UActorComponent](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/UActorComponent) | Ownership, validity/creation state, and visualization-helper identification. |
| [UBlueprintEditorSettings](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/BlueprintGraph/UBlueprintEditorSettings) | The live construction-script component visibility preference. Its header belongs to BlueprintGraph, which is an explicit module dependency. |
| [FComponentEditorUtils](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/UnrealEd/FComponentEditorUtils) | Checking whether a native component has an editable component property. |
| [IPropertyUtilities](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Editor/PropertyEditor/IPropertyUtilities) | Deferred selection actions and requested Details refresh. |
| [IModuleInterface](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Core/IModuleInterface) | Startup/shutdown and reload policy. |

The module dependencies are Core, CoreUObject, Engine, Slate, SlateCore, PropertyEditor, DetailCustomizations, UnrealEd, and BlueprintGraph. Optional registration state uses `Misc/Optional.h`.

## Engine source references

Paths below are relative to `Engine/Source/Editor`. Surface uses public interfaces and does not require modifying or distributing these engine files.

| Source | Relevant behavior |
| --- | --- |
| `PropertyEditor/Private/DetailLayoutBuilderImpl.cpp` and `.h` | Category ownership, layout generation, and public-interface boundaries relevant to favorite collection. |
| `PropertyEditor/Private/DetailCategoryBuilderImpl.cpp` and `.h` | External-object root hiding, category expansion, custom headers, category generation, and saved expansion lookup. |
| `PropertyEditor/Private/DetailPropertyRow.cpp` | Embedded external-object layouts are generated during row construction; native component customizations participate in that layout. |
| `PropertyEditor/Private/SDetailsViewBase.cpp` | Native favorite category creation, customization lifecycle, and restoration of custom expansion entries. |
| `PropertyEditor/Private/DetailItemNode.cpp` | Child-node flattening, node initialization, and expansion save/restore behavior. |
| `PropertyEditor/Private/SDetailSingleItemRow.cpp` and `DetailLayoutHelpers.cpp` | Native property row rendering and favorite/layout handling. |
| `PropertyEditor/Private/SDetailCategoryTableRow.cpp` and `.h` | Whole-row header content replaces the default category title while retaining the native expander area. |
| `SubobjectEditor/Private/SSubobjectEditor.cpp` and `SSubobjectInstanceEditor.cpp` | Component tree construction delegates to subobject data gathering; additional UI customization can filter the resulting entries. |
| `SubobjectDataInterface/Private/SubobjectDataSubsystem.cpp` | Stock level-instance component inclusion checks and unconditional root-component inclusion. |
| `PropertyEditor/Private/DetailGroup.cpp` | Native grouping of favorite property rows. |

## Implementation

### Preserve existing controls and insert Surface categories

Surface composes with existing Details customization behavior. It adds `Surface_Favorites` and `Surface_Components`, displayed as **Surface - Favorites** and **Surface - Component Details**. Direct component selection uses a separate Surface navigation category.

The sort callback places Surface after `TransformCommon` or `Transform` when available and shifts subsequent category sort indices while retaining their relative order.

Category headers combine native style brushes (`Icons.Star` and `Icons.Search`) with foreground tint and text. The component-body header is a custom row with an explicit expansion button. Native external-object roots are hidden, with native categories displayed beneath the custom header.

### Collect component favorites into a separate category

During embedded component layout generation, Surface registers a read-only category-sort callback. In UE 5.8, this callback reads the completed native component Favorites category. Surface validates each property handle's favorite flag and outer objects, deduplicates handles, and adds normal property rows under the matching component group in Surface - Favorites.

Collected component favorites appear in Surface - Favorites, separately from the Actor's built-in Favorites category. These rows follow native class/property favorite behavior and are independent of Surface's component filters.

Favorite collection depends on the engine's layout-generation sequence. Recheck that sequence when upgrading engines, and inspect `LogSurfaceFavorites` if collection does not complete.

### Follow stock initial visibility rules

For a newly encountered component, Surface mirrors the stock level-instance predicate from `USubobjectDataSubsystem::GatherSubobjectData`, including its root exception:

1. Include the Actor's root component by default.
2. Otherwise exclude visualization components.
3. Respect `bHideConstructionScriptComponentsInDetailsView` for user-construction-script components.
4. Exclude nested default subobjects attached beneath construction-script-created scene components.
5. Exclude native components without a property returned by `GetPropertyForEditableNativeComponent`.

The result sets an initial checkbox state and explanatory tooltip. It neither removes the entry from Surface's menu nor changes property editability. For a matched multi-selection group, a default exclusion on any member defaults the group to unchecked. Previously stored choices take precedence. Additional subsystem adapters or component-tree customizations are outside this predicate.

### Own Surface's UI preferences explicitly

`SurfacePanelControls.h` retains weak Actor references and per-view, per-selection state in memory. Search, known/hidden component IDs, expanded component IDs, and the two Surface category expansion booleans are stored through `GConfig` in `GEditorPerProjectIni`.

The section key uses a hash of sorted level/Actor identities. Component identity remains the matching key: exact class path, relative object path, and creation method. Defaults apply only to previously unknown IDs. Search typing updates the config cache; committed search and explicit preference changes flush settings as implemented. UI persistence does not write to Actor assets.

Inner native property expansion and native favorite choices remain engine-managed. Separate views can hold different in-memory preferences for the same selection, but share its persistent identity; the latest saved state is what a later session loads.

### Component layout and indentation

Each component uses a custom expansion header followed by an external property row with its object root hidden. These rows are siblings in the containing category, rather than a native parent/child component node. Native property categories can therefore appear less indented than the component header, while still displaying that component's properties.

Surface stores the custom component header's expanded/collapsed state explicitly. Inner native property-category expansion remains managed by Unreal.

Surface requires no engine modifications, generated UObject classes, or runtime module.
