#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MetaHumanSweatTypes.h"
#include "MetaHumanSweatComponent.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USkeletalMesh;
class USkeletalMeshComponent;
struct FStreamableHandle;

/** One skin material slot this component has taken over. Internal. */
USTRUCT()
struct FMetaHumanSweatBinding
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> Component;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMesh> Mesh;

	/** The sweat material instance drawn in the slot. Owned by the transient package. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	/** What the component's override for the slot held before (null when the slot used the mesh's own material). Put back on release. */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> OriginalOverride;

	int32 Slot = INDEX_NONE;
	bool bApplied = false;
	FMetaHumanSweatParams Last;
};

/**
 * Sweat on a MetaHuman's skin. Add one to the character's actor (or its Blueprint).
 *
 * The character carries two levels from 0 to 1. Sweat rises with Effort and draws a damp sheen, beads and drops that run downhill;
 * Wetness rises with Rain Exposure and Immersion and draws a thin film with fine droplets. Both dry by themselves. With no sweat the
 * skin is matte.
 *
 * Before it can show anything, the character's skin materials need their sweat versions: select the character in the level and run
 * Tools > MetaHuman Sweat > Add Sweat to Selected Actors (once per character; it also adds this component). At runtime the component
 * finds those materials by name, puts a dynamic instance of each on the skin slots of every skeletal mesh component of its actor,
 * and writes the levels and the look into them. It restores the original materials when it ends or is disabled.
 *
 * Drive it from Blueprint (Set Effort, Set Rain Exposure, Set Immersion, Set Sweat Level, Dry Now) or let it read the actor's speed
 * (Effort From Speed). Console: mhs.Enabled 0|1, mhs.Debug 0|1, mhs.Exert <0-1>, mhs.SetSweat <0-1>, mhs.Dry.
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent, DisplayName = "MetaHuman Sweat", ToolTip = "Sweat on a MetaHuman's skin: matte when dry, then a damp sheen, beads and running drops. Prepare the character once with Tools > MetaHuman Sweat > Add Sweat to Selected Actors."))
class METAHUMANSWEAT_API UMetaHumanSweatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMetaHumanSweatComponent();

	/** Off restores the original skin materials and stops the model. Default on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat", meta = (ToolTip = "Off restores the character's original skin materials and stops the sweat model (the levels are kept). Default on."))
	bool bSweatEnabled = true;

	/** How hard the character is working, 0 (rest) to 1 (flat out). Sweat builds at Sweat Build Rate x Effort. Set it from gameplay, or use Effort From Speed. Default 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How hard the character is working: 0 at rest, 1 flat out. Sweat builds at Sweat Build Rate x Effort and dries by itself. Set it from gameplay (Set Effort), or turn on Effort From Speed. Range 0 to 1. Default 0."))
	float Effort = 0.f;

	/** How exposed the character is to rain, 0 (dry or under cover) to 1 (in a downpour). Feeds Wetness. The plugin does not read any weather system: set it from yours. Default 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How exposed the character is to rain: 0 dry or under cover, 1 in a downpour. Feeds the Wetness level (a thin film and fine droplets). The plugin reads no weather system: set this from yours. Range 0 to 1. Default 0."))
	float RainExposure = 0.f;

	/** How far the character is under water, 0 (out) to 1 (submerged). Feeds Wetness quickly. Set it from your swimming or water code. Default 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "How far the character is under water: 0 out of the water, 1 submerged. Feeds the Wetness level quickly; the character then dries slowly. Set it from your swimming or water code. Range 0 to 1. Default 0."))
	float Immersion = 0.f;

	/** On: effort also follows the actor's speed, speed / Full Effort Speed (the larger of that and Effort is used). Default off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ToolTip = "On: effort also follows the actor's speed, as speed / Full Effort Speed; the larger of that and Effort is used. A simple proxy: a character working hard on the spot needs Effort set by gameplay. Default off."))
	bool bEffortFromSpeed = false;

	/** Speed, in cm/s, that counts as full effort while Effort From Speed is on. Range 50 to 2000, default 500 (about a run). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ClampMin = "50.0", ClampMax = "2000.0", EditCondition = "bEffortFromSpeed", ToolTip = "Speed, in cm/s, that counts as full effort while Effort From Speed is on: at this speed the character sweats at the full rate, at half the speed at half the rate. Range 50 to 2000. Default and recommended 500, about a run; 150 (a walk) soaks everyone who walks."))
	float FullEffortSpeed = 500.f;

	/** Sweat level the character starts with, 0 to 1. Default 0 (dry, matte skin). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Inputs", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Sweat level the character starts with when play begins. 0 is dry, matte skin; 0.3 a light sheen with small beads; 0.7 drenched with running drops. Range 0 to 1. Default 0."))
	float InitialSweat = 0.f;

	/** On: this character uses Look below instead of the look in Project Settings > Plugins > MetaHuman Sweat. Default off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Look", meta = (ToolTip = "On: this character uses the Look below instead of the one in Project Settings > Plugins > MetaHuman Sweat. Default off."))
	bool bOverrideLook = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Look", meta = (EditCondition = "bOverrideLook", ToolTip = "This character's own look: intensity, bead density and size, streak length. Used while Override Look is on."))
	FMetaHumanSweatLook Look;

	/** On: this character uses Rates below instead of the rates in Project Settings > Plugins > MetaHuman Sweat. Default off. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Pace", meta = (ToolTip = "On: this character uses the Rates below instead of the ones in Project Settings > Plugins > MetaHuman Sweat. Default off."))
	bool bOverrideRates = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sweat|Pace", meta = (EditCondition = "bOverrideRates", ToolTip = "This character's own rates: how fast it sweats, gets wet and dries. Used while Override Rates is on."))
	FMetaHumanSweatRates Rates;

	/** Turns the sweat on or off for this character. Off restores the original skin materials on the next tick; the levels are kept. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Turns the sweat on or off for this character. Off restores the original skin materials on the next tick and stops the model; the levels are kept, so turning it on again carries on where it stopped."))
	void SetSweatEnabled(bool bEnabled);

	/** Sets how hard the character is working, 0 to 1. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Sets how hard the character is working: 0 at rest, 1 flat out. Sweat builds while it is above 0 and dries by itself."))
	void SetEffort(float NewEffort);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Sets how exposed the character is to rain: 0 dry or under cover, 1 in a downpour."))
	void SetRainExposure(float NewRainExposure);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Sets how far the character is under water: 0 out, 1 submerged."))
	void SetImmersion(float NewImmersion);

	/** Jumps the sweat level to a value, 0 to 1, bypassing the build-up. The model carries on from there. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Jumps the sweat level to a value from 0 to 1, bypassing the build-up (for a cutscene or a character who arrives already sweating). The model carries on from there."))
	void SetSweatLevel(float NewSweat);

	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Jumps the wetness level (rain and immersion) to a value from 0 to 1. The model carries on from there."))
	void SetWetnessLevel(float NewWetness);

	/** Both levels to 0 at once: dry, matte skin. */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Sets the sweat and wetness levels to 0 at once: dry, matte skin."))
	void DryNow();

	UFUNCTION(BlueprintPure, Category = "MetaHuman Sweat", meta = (ToolTip = "The current sweat level, 0 to 1."))
	float GetSweatLevel() const { return static_cast<float>(Model.Sweat); }

	UFUNCTION(BlueprintPure, Category = "MetaHuman Sweat", meta = (ToolTip = "The current wetness level from rain and immersion, 0 to 1."))
	float GetWetnessLevel() const { return static_cast<float>(Model.Wetness); }

	/** How many skin material slots currently draw a sweat material. 0 means nothing can show: see Get Sweat Status. */
	UFUNCTION(BlueprintPure, Category = "MetaHuman Sweat", meta = (ToolTip = "How many skin material slots currently draw a sweat material. 0 means nothing can show: Get Sweat Status says why."))
	int32 GetBoundSlotCount() const;

	/** One line for a debug display: levels, bound slots and, when nothing is bound, why. */
	UFUNCTION(BlueprintPure, Category = "MetaHuman Sweat", meta = (ToolTip = "One line for a debug display: the levels, how many skin slots are bound and, when none is, the reason (for example that the character's skin materials have no sweat versions yet)."))
	FString GetSweatStatus() const;

	/** Looks at the actor's skeletal mesh components again now (the component also does this once a second). */
	UFUNCTION(BlueprintCallable, Category = "MetaHuman Sweat", meta = (ToolTip = "Looks at the actor's skeletal mesh components again now and takes over any new skin slot. The component also does this by itself once a second, so call it only to avoid that delay after swapping a mesh or material."))
	void RefreshSkinSlots();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	struct FSweatLoad
	{
		FString ObjectPath;
		TSharedPtr<FStreamableHandle> Handle;
		bool bMissing = false;
	};

	void BindSlots();
	void BindComponent(USkeletalMeshComponent* Component);
	/** The sweat material for a slot's material, null while it loads or when there is none. */
	UMaterialInterface* ResolveSweatMaterial(UMaterialInterface* Source);
	void ReleaseAll();
	void PushParameters();
	const FMetaHumanSweatLook& ActiveLook() const;
	const FMetaHumanSweatRates& ActiveRates() const;

	UPROPERTY(Transient)
	TArray<FMetaHumanSweatBinding> Bindings;

	TArray<FSweatLoad> Loads;
	FMetaHumanSweatModel Model;
	FString Note;
	float Seed = 0.f;
	float RescanIn = 0.f;
	int32 SkinSlotsSeen = 0;
	bool bLoadsPending = false;
};
