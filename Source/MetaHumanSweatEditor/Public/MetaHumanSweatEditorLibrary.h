#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaHumanSweatEditorLibrary.generated.h"

class AActor;
class USkeletalMesh;

/**
 * The editor side of MetaHuman Sweat: makes the sweat versions of a character's skin materials. Behind Tools > MetaHuman Sweat, and
 * callable from editor Blueprints and Python (unreal.MetaHumanSweatEditorLibrary).
 *
 * For every skin material instance it finds (one whose base material is named in Project Settings > Plugins > MetaHuman Sweat > Skin
 * Material Names), it creates, under <Material Folder>/Materials:
 *   M_Sweat_<Master>_<Hash>     a copy of the skin master material with the sweat block added in front of its output (once per master);
 *   MI_Sweat_<Name>_<Hash>      a copy of every material instance between the master and the character's skin material, re-parented so
 *                               the chain ends at the sweat master. The last one carries the face or body mask and the bead cell count
 *                               measured on the mesh.
 * and imports the two masks into <Material Folder>/Textures. Nothing the character already uses is changed: the MetaHuman Sweat
 * component swaps the materials in at runtime and puts the originals back.
 *
 * Every function returns a plain-text report, one line per thing done or skipped, and logs it (LogMetaHumanSweat).
 */
UCLASS()
class METAHUMANSWEATEDITOR_API UMetaHumanSweatEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Makes the sweat materials for the skin slots of these skeletal meshes (a MetaHuman's body and face meshes). Safe to run again: existing materials are kept and refreshed. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Makes the sweat materials for the skin slots of these skeletal meshes (a MetaHuman's body and face meshes). Safe to run again: what exists is kept and refreshed. Returns a report."))
	static FString AddSweatToMeshes(const TArray<USkeletalMesh*>& Meshes);

	/** Makes the sweat materials for every skeletal mesh component of these level actors and, with bAddComponent, gives each actor a MetaHuman Sweat component if it has none. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Makes the sweat materials for every skeletal mesh component of these level actors and, with Add Component on, gives each actor a MetaHuman Sweat component if it has none (as an instance component: add it in the Blueprint instead to have it on every copy). Returns a report."))
	static FString AddSweatToActors(const TArray<AActor*>& Actors, bool bAddComponent = true);

	/** Rebuilds the sweat block of every sweat master material from the plugin's current shader files and recompiles it. Run it after updating the plugin or editing Resources/Shaders. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Rebuilds the sweat block of every sweat master material from the plugin's current shader files (Resources/Shaders) and recompiles it. Run it after updating the plugin or editing those files. Returns a report."))
	static FString UpdateSweatMaterials();
};
