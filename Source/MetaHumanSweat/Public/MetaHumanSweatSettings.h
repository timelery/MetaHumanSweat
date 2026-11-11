#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MetaHumanSweatTypes.h"
#include "MetaHumanSweatSettings.generated.h"

/**
 * Project Settings > Plugins > MetaHuman Sweat. The defaults every MetaHuman Sweat component uses unless it overrides them, and where the
 * editor tools put the sweat materials they create.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "MetaHuman Sweat"))
class METAHUMANSWEAT_API UMetaHumanSweatSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMetaHumanSweatSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** How sweat looks on every character that does not override it on its component. */
	UPROPERTY(config, EditAnywhere, Category = "Look", meta = (ToolTip = "How sweat looks on every character whose MetaHuman Sweat component does not override the look. All four multipliers are at 1 for the tuned look."))
	FMetaHumanSweatLook Look;

	/** How fast characters sweat, get wet and dry, unless a component overrides it. */
	UPROPERTY(config, EditAnywhere, Category = "Pace", meta = (ToolTip = "How fast characters sweat, get wet and dry, for every MetaHuman Sweat component that does not override the rates. Illustrative rates, not physiology."))
	FMetaHumanSweatRates Rates;

	/** Content folder the editor tools write to and the component reads from: <Folder>/Materials and <Folder>/Textures. Changing it after materials exist orphans them: move the folder too. Default /Game/MetaHumanSweat. */
	UPROPERTY(config, EditAnywhere, Category = "Materials", meta = (ToolTip = "Content folder the editor tools write the sweat materials and masks to, and the component looks them up in: <Folder>/Materials and <Folder>/Textures. If you change it after materials exist, move the folder in the Content Browser too. Default /Game/MetaHumanSweat."))
	FString MaterialFolder = TEXT("/Game/MetaHumanSweat");

	/** Asset names of the skin master materials the editor tools recognise. A material instance whose base material has one of these names is skin. Default: M_skin_unified_baked, the skin of characters assembled by MetaHuman Creator. */
	UPROPERTY(config, EditAnywhere, Category = "Materials", meta = (ToolTip = "Asset names of the skin master materials the editor tools recognise: a material instance whose base material has one of these names is treated as skin. The master must use Material Attributes. Default: M_skin_unified_baked, the skin of characters assembled by MetaHuman Creator."))
	TArray<FString> SkinMaterialNames;

	/** Size of one bead cell on the skin, in cm. The tools measure each mesh's UV scale and store the matching cell count on its sweat material. Smaller cells give smaller, denser beads. Range 0.2 to 2, default 0.6. Applies to materials created or updated afterwards. */
	UPROPERTY(config, EditAnywhere, Category = "Materials", meta = (ClampMin = "0.2", ClampMax = "2.0", ToolTip = "Size of one bead cell on the skin, in cm. The editor tools measure each mesh's UV scale and store the matching cell count on its sweat material, so beads are the same size on every body. Smaller cells give smaller, denser beads. Range 0.2 to 2. Default and recommended 0.6. Applies to materials created or updated afterwards (Tools > MetaHuman Sweat)."))
	float BeadCellSizeCm = 0.6f;

	static const UMetaHumanSweatSettings& Get() { return *GetDefault<UMetaHumanSweatSettings>(); }

	/** MaterialFolder without a trailing slash; the default when it is empty. */
	FString GetFolder() const;
};
