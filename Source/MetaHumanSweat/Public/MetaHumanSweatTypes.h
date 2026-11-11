#pragma once

#include "CoreMinimal.h"
#include "MetaHumanSweatTypes.generated.h"

class UMaterialInstanceDynamic;

METAHUMANSWEAT_API DECLARE_LOG_CATEGORY_EXTERN(LogMetaHumanSweat, Log, All);

/** How sweat looks on the skin. Every value scales the picture; none changes how fast a character sweats. */
USTRUCT(BlueprintType)
struct METAHUMANSWEAT_API FMetaHumanSweatLook
{
	GENERATED_BODY()

	/** How strongly a given sweat level shows: scales the sweat fed to the skin shader. 0 hides it, 2 doubles how fast the sheen, beads and streaks build. Range 0 to 2, default and recommended 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.0", ClampMax = "2.0", ToolTip = "How strongly a given sweat level shows: scales the sweat fed to the skin shader. 0 hides it, 2 doubles how fast the sheen, beads and streaks build. Range 0 to 2. Default and recommended 1 (usual 0.7 to 1.3)."))
	float Intensity = 1.f;

	/** How many beads per area. 1 is about one bead cell per 6 mm of skin; 4 packs four times as many, 0.25 a quarter. Range 0.25 to 4, default and recommended 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.25", ClampMax = "4.0", ToolTip = "How many beads per area. 1 is about one bead cell per 6 mm of skin; 4 packs four times as many, 0.25 a quarter. Dense, large beads read as a rash: raise it only together with a smaller Bead Size. Range 0.25 to 4. Default and recommended 1 (usual 0.6 to 1.6)."))
	float BeadDensity = 1.f;

	/** Bead radius multiplier. A bead always stays inside its own cell, so large values stop growing. Range 0.25 to 3, default and recommended 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.25", ClampMax = "3.0", ToolTip = "Bead radius multiplier. At 1 a bead is about 0.3 mm across at the first trace of sweat and up to 3.5 mm at full sweat. A bead always stays inside its own cell, so values above about 2 stop growing; above 1.5 beads begin to read as spots. Range 0.25 to 3. Default and recommended 1 (usual 0.7 to 1.4)."))
	float BeadSize = 1.f;

	/** Length of the running drops' trails. 0 switches the streaks off and leaves the sheen and beads. 1 is a run of about 1 to 2 cm. Range 0 to 3, default and recommended 1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.0", ClampMax = "3.0", ToolTip = "Length of the trails the running drops leave. At 1 a drop runs about 1 to 2 cm in 5 to 14 seconds, fades and starts again. 0 switches the streaks off and leaves the sheen and beads. Range 0 to 3. Default and recommended 1 (usual 0.6 to 1.6)."))
	float StreakLength = 1.f;

	/** Orientation fix for the streaks and the beads' relief: 0 none (correct for MetaHuman skin), 1 flips U, 2 flips V, 3 both. Range 0 to 3, default 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0", ClampMax = "3", ToolTip = "Orientation fix for the streaks and the relief of the beads: flips the sense of the skin UV axes in the shader. 0 none (correct for MetaHuman skin), 1 flips U, 2 flips V, 3 both. Change it only if drops run uphill or beads look like dents on a custom body. Range 0 to 3. Default 0."))
	int32 FlowFlip = 0;

	/** The same look with every value brought into its range (a non-finite value becomes the default). */
	FMetaHumanSweatLook Clamped() const;
};

/** How fast the two levels a character carries move: Sweat (from effort) and Wetness (from rain and immersion). Rates are per second. Illustrative, not physiological values. */
USTRUCT(BlueprintType)
struct METAHUMANSWEAT_API FMetaHumanSweatRates
{
	GENERATED_BODY()

	/** How fast sweat builds, per second at full effort (half as fast at half effort). The level climbs towards build / (build + dry): 0.73 at the defaults. Range 0.005 to 0.5, default 0.04. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.005", ClampMax = "0.5", ToolTip = "How fast sweat builds, per second at full effort; half as fast at half effort. The sweat level climbs towards build / (build + dry), 0.73 at the defaults and never 1, and gets two thirds of the way there in 1 / (build + dry) seconds, 18 s at the defaults. Range 0.005 to 0.5. Default and recommended 0.04 (usual 0.02 to 0.1)."))
	float SweatBuildRate = 0.04f;

	/** How fast sweat dries, per second, at any effort. At rest the level halves every 0.69 / rate seconds: 46 s at the default. Range 0.002 to 0.2, default 0.015. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ClampMin = "0.002", ClampMax = "0.2", ToolTip = "How fast sweat dries, per second, whatever the character is doing. At rest the sweat level halves every 0.69 / rate seconds: 46 s at the default. A higher rate also lowers the level full effort can reach. Range 0.002 to 0.2. Default and recommended 0.015 (usual 0.005 to 0.05)."))
	float SweatDryRate = 0.015f;

	/** How fast rain wets the skin, per second at full rain exposure. Range 0.005 to 1, default 0.08. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wetness", meta = (ClampMin = "0.005", ClampMax = "1.0", ToolTip = "How fast rain wets the skin, per second at a Rain Exposure of 1. Range 0.005 to 1. Default 0.08, about 12 s to two thirds wet."))
	float RainRate = 0.08f;

	/** How fast being in water wets the skin, per second at an Immersion of 1. Range 0.05 to 5, default 0.8. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wetness", meta = (ClampMin = "0.05", ClampMax = "5.0", ToolTip = "How fast being in water wets the skin, per second at an Immersion of 1. Range 0.05 to 5. Default 0.8, wet in about a second."))
	float ImmersionRate = 0.8f;

	/** How fast rain and immersion wetness dries, per second. The level halves every 0.69 / rate seconds: 69 s at the default. Range 0.001 to 0.2, default 0.01. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wetness", meta = (ClampMin = "0.001", ClampMax = "0.2", ToolTip = "How fast the wetness from rain and immersion dries, per second. The level halves every 0.69 / rate seconds: 69 s at the default. Range 0.001 to 0.2. Default 0.01."))
	float WetnessDryRate = 0.01f;

	FMetaHumanSweatRates Clamped() const;
};

/**
 * The two levels a character carries, each 0 to 1, and how inputs move them. A plain value type: no clock, no UObject, no world.
 * dx/dt = Production * (1 - x) - Removal * x, advanced in closed form, so a constant-input interval gives the same result however it is subdivided.
 */
struct METAHUMANSWEAT_API FMetaHumanSweatModel
{
	/** 0 to 1. Rises with effort, falls all the time. Draws the sheen, the beads and the streaks. */
	double Sweat = 0.0;
	/** 0 to 1. Rises with rain exposure and immersion, falls all the time. Draws a thin damp film and fine droplets everywhere. */
	double Wetness = 0.0;

	/** Advances both levels by Seconds (at most 60 are counted). Inputs are clamped to 0..1; a non-finite input counts as 0. */
	void Advance(double Seconds, double Effort, double RainExposure, double Immersion, const FMetaHumanSweatRates& Rates);
};

/** What the skin material is given. */
struct METAHUMANSWEAT_API FMetaHumanSweatParams
{
	float Amount = 0.f;
	float Wetness = 0.f;
	float Intensity = 1.f;
	float Density = 1.f;
	float BeadSize = 1.f;
	float StreakLength = 1.f;
	float Seed = 0.f;
	FLinearColor FlowAxes = FLinearColor(1.f, 1.f, 0.f, 0.f);

	bool NearlyEquals(const FMetaHumanSweatParams& Other, float Tolerance = 0.002f) const;
};

namespace MetaHumanSweat
{
	/** Names of the parameters of a sweat skin material. The component writes the first eight; Cells and Mask are set per material by the editor tools; WorldUp keeps its default. */
	namespace Param
	{
		inline const FName Amount(TEXT("MHSweatAmount"));
		inline const FName Wetness(TEXT("MHSweatWetness"));
		inline const FName Intensity(TEXT("MHSweatIntensity"));
		inline const FName Density(TEXT("MHSweatDensity"));
		inline const FName BeadSize(TEXT("MHSweatBeadSize"));
		inline const FName StreakLength(TEXT("MHSweatStreakLength"));
		inline const FName Seed(TEXT("MHSweatSeed"));
		inline const FName FlowAxes(TEXT("MHSweatFlowAxes"));
		inline const FName Cells(TEXT("MHSweatCells"));
		inline const FName Mask(TEXT("MHSweatMask"));
		inline const FName WorldUp(TEXT("MHSweatWorldUp"));
	}

	/** Model output, look and seed as shader parameters. */
	METAHUMANSWEAT_API FMetaHumanSweatParams MapParameters(const FMetaHumanSweatModel& Model, const FMetaHumanSweatLook& Look, float Seed);
	METAHUMANSWEAT_API void ApplyParameters(UMaterialInstanceDynamic* Material, const FMetaHumanSweatParams& Params);

	/** A stable 0..1 value from a name, so two characters do not share one bead pattern. */
	METAHUMANSWEAT_API float SeedFromName(const FString& Name);

	/** FNV-1a 32 of the lower-cased UTF-8 text. */
	METAHUMANSWEAT_API uint32 PathHash(const FString& Text);

	/** "<Prefix>_<Name, at most 40 safe characters>_<8 hex digits of PathHash(PackageName)>": the asset name of the sweat copy of a material. */
	METAHUMANSWEAT_API FString SweatAssetName(const TCHAR* Prefix, const FString& SourcePackageName, const FString& SourceAssetName);

	/**
	 * Object path of the sweat material instance that stands in for a dry skin material instance: "<Folder>/Materials/MI_Sweat_<Name>_<Hash>.<same>".
	 * The editor tools create it under this name and the component looks it up by it, so no table links the two.
	 */
	METAHUMANSWEAT_API FString SweatMaterialObjectPath(const FString& Folder, const FString& DryPackageName, const FString& DryAssetName);
}
