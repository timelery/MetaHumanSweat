#include "MetaHumanSweatComponent.h"

#include "MetaHumanSweatSettings.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanSweatComponent)

namespace MetaHumanSweatConsole
{
	extern TAutoConsoleVariable<int32> CVarEnabled;
	extern TAutoConsoleVariable<int32> CVarDebug;
}

namespace MetaHumanSweatComponentPrivate
{
	constexpr int32 MaxSlotsPerComponent = 64;
	constexpr float RescanSeconds = 1.f;

	/** The asset a slot's material stands for: itself, or, for a dynamic instance made at runtime, the first parent that is an asset. */
	UMaterialInterface* AssetOf(UMaterialInterface* Material)
	{
		UMaterialInterface* Current = Material;
		for (int32 Depth = 0; Current && Depth < 16; ++Depth)
		{
			UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Current);
			if (!Dynamic)
			{
				return Current;
			}
			Current = Dynamic->Parent;
		}
		return nullptr;
	}
}

UMetaHumanSweatComponent::UMetaHumanSweatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After animation and movement, so Effort From Speed reads this frame's velocity.
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

const FMetaHumanSweatLook& UMetaHumanSweatComponent::ActiveLook() const
{
	return bOverrideLook ? Look : UMetaHumanSweatSettings::Get().Look;
}

const FMetaHumanSweatRates& UMetaHumanSweatComponent::ActiveRates() const
{
	return bOverrideRates ? Rates : UMetaHumanSweatSettings::Get().Rates;
}

void UMetaHumanSweatComponent::BeginPlay()
{
	Super::BeginPlay();
	const AActor* Owner = GetOwner();
	Seed = Owner ? MetaHumanSweat::SeedFromName(Owner->GetName()) : 0.f;
	Model.Sweat = FMath::Clamp(static_cast<double>(InitialSweat), 0.0, 1.0);
	Model.Wetness = 0.0;
	RescanIn = 0.f;
}

void UMetaHumanSweatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAll();
	Loads.Reset();
	Super::EndPlay(EndPlayReason);
}

void UMetaHumanSweatComponent::SetSweatEnabled(bool bEnabled)
{
	bSweatEnabled = bEnabled;
}

void UMetaHumanSweatComponent::SetEffort(float NewEffort)
{
	Effort = FMath::IsFinite(NewEffort) ? FMath::Clamp(NewEffort, 0.f, 1.f) : 0.f;
}

void UMetaHumanSweatComponent::SetRainExposure(float NewRainExposure)
{
	RainExposure = FMath::IsFinite(NewRainExposure) ? FMath::Clamp(NewRainExposure, 0.f, 1.f) : 0.f;
}

void UMetaHumanSweatComponent::SetImmersion(float NewImmersion)
{
	Immersion = FMath::IsFinite(NewImmersion) ? FMath::Clamp(NewImmersion, 0.f, 1.f) : 0.f;
}

void UMetaHumanSweatComponent::SetSweatLevel(float NewSweat)
{
	Model.Sweat = FMath::IsFinite(NewSweat) ? FMath::Clamp(static_cast<double>(NewSweat), 0.0, 1.0) : 0.0;
}

void UMetaHumanSweatComponent::SetWetnessLevel(float NewWetness)
{
	Model.Wetness = FMath::IsFinite(NewWetness) ? FMath::Clamp(static_cast<double>(NewWetness), 0.0, 1.0) : 0.0;
}

void UMetaHumanSweatComponent::DryNow()
{
	Model.Sweat = 0.0;
	Model.Wetness = 0.0;
}

int32 UMetaHumanSweatComponent::GetBoundSlotCount() const
{
	int32 Count = 0;
	for (const FMetaHumanSweatBinding& Binding : Bindings)
	{
		if (Binding.Component && Binding.Material && Binding.Component->GetMaterial(Binding.Slot) == Binding.Material)
		{
			++Count;
		}
	}
	return Count;
}

FString UMetaHumanSweatComponent::GetSweatStatus() const
{
	const int32 Bound = GetBoundSlotCount();
	FString Text = FString::Printf(TEXT("%s: sweat %.2f, wetness %.2f, %d skin slot(s) bound"), *GetNameSafe(GetOwner()), Model.Sweat, Model.Wetness, Bound);
	if (!bSweatEnabled)
	{
		Text += TEXT(" (Sweat Enabled is off)");
	}
	else if (MetaHumanSweatConsole::CVarEnabled.GetValueOnGameThread() == 0)
	{
		Text += TEXT(" (mhs.Enabled is 0)");
	}
	else if (!Note.IsEmpty())
	{
		Text += TEXT(" - ") + Note;
	}
	return Text;
}

void UMetaHumanSweatComponent::RefreshSkinSlots()
{
	RescanIn = 0.f;
}

void UMetaHumanSweatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bOn = bSweatEnabled && MetaHumanSweatConsole::CVarEnabled.GetValueOnGameThread() != 0;
	if (!bOn)
	{
		if (!Bindings.IsEmpty())
		{
			ReleaseAll();
		}
		RescanIn = 0.f;
		return;
	}

	double EffortNow = FMath::IsFinite(Effort) ? FMath::Clamp(static_cast<double>(Effort), 0.0, 1.0) : 0.0;
	if (bEffortFromSpeed && FullEffortSpeed > 0.f)
	{
		if (const AActor* Owner = GetOwner())
		{
			EffortNow = FMath::Max(EffortNow, FMath::Clamp(Owner->GetVelocity().Size() / static_cast<double>(FullEffortSpeed), 0.0, 1.0));
		}
	}
	Model.Advance(DeltaTime, EffortNow, RainExposure, Immersion, ActiveRates());

	RescanIn -= DeltaTime;
	if (RescanIn <= 0.f || bLoadsPending)
	{
		BindSlots();
		RescanIn = MetaHumanSweatComponentPrivate::RescanSeconds;
	}
	PushParameters();

	if (MetaHumanSweatConsole::CVarDebug.GetValueOnGameThread() != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 0.f, GetBoundSlotCount() > 0 ? FColor::Cyan : FColor::Orange,
			FString::Printf(TEXT("MetaHuman Sweat  %s  effort %.2f"), *GetSweatStatus(), EffortNow));
	}
}

UMaterialInterface* UMetaHumanSweatComponent::ResolveSweatMaterial(UMaterialInterface* Source)
{
	// Only material instance assets have a sweat version: the editor tools copy the instance chain of a skin material.
	if (!Source || !Source->IsAsset() || !Cast<UMaterialInstance>(Source))
	{
		return nullptr;
	}
	const FString Path = MetaHumanSweat::SweatMaterialObjectPath(UMetaHumanSweatSettings::Get().GetFolder(), Source->GetOutermost()->GetName(), Source->GetName());
	FSweatLoad* Load = Loads.FindByPredicate([&Path](const FSweatLoad& Item) { return Item.ObjectPath == Path; });
	if (!Load)
	{
		Load = &Loads.AddDefaulted_GetRef();
		Load->ObjectPath = Path;
		const FSoftObjectPath SoftPath(Path);
		if (!IAssetRegistry::GetChecked().GetAssetByObjectPath(SoftPath).IsValid())
		{
			// Not a skin material, or its sweat version was never made. Most slots of a character (eyes, teeth, clothes) end here.
			Load->bMissing = true;
		}
		else
		{
			TArray<FSoftObjectPath> Paths;
			Paths.Add(SoftPath);
			Load->Handle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths);
			if (!Load->Handle.IsValid())
			{
				Load->bMissing = true;
				UE_LOGF(LogMetaHumanSweat, Warning, "Could not start loading the sweat material %ls", *Path);
			}
		}
	}
	if (Load->bMissing)
	{
		return nullptr;
	}
	if (!Load->Handle->HasLoadCompleted())
	{
		bLoadsPending = true;
		return nullptr;
	}
	UMaterialInterface* Sweat = Cast<UMaterialInterface>(Load->Handle->GetLoadedAsset());
	float Unused = 0.f;
	if (!Sweat || Cast<UMaterialInstanceDynamic>(Sweat) || !Sweat->GetScalarParameterValue(FHashedMaterialParameterInfo(MetaHumanSweat::Param::Amount), Unused))
	{
		Load->bMissing = true;
		UE_LOGF(LogMetaHumanSweat, Warning, "%ls is not a usable sweat material (it has no %ls parameter): run Tools > MetaHuman Sweat > Update Sweat Materials. The slot stays dry.",
			*Path, *MetaHumanSweat::Param::Amount.ToString());
		return nullptr;
	}
	return Sweat;
}

void UMetaHumanSweatComponent::BindComponent(USkeletalMeshComponent* Component)
{
	if (!IsValid(Component) || !Component->GetSkeletalMeshAsset())
	{
		return;
	}
	const int32 SlotCount = FMath::Min(Component->GetNumMaterials(), MetaHumanSweatComponentPrivate::MaxSlotsPerComponent);
	for (int32 Slot = 0; Slot < SlotCount; ++Slot)
	{
		if (Bindings.ContainsByPredicate([Component, Slot](const FMetaHumanSweatBinding& Item) { return Item.Component == Component && Item.Slot == Slot; }))
		{
			continue;
		}
		UMaterialInterface* Current = Component->GetMaterial(Slot);
		UMaterialInterface* Source = MetaHumanSweatComponentPrivate::AssetOf(Current);
		if (UMaterialInstanceDynamic* Orphan = Cast<UMaterialInstanceDynamic>(Current))
		{
			// A dynamic instance of a sweat material that is not in our list: one of ours that outlived its component (re-running a Blueprint's
			// construction script rebuilds the mesh component and copies its material overrides across). Take it back instead of leaving it stuck.
			float Unused = 0.f;
			if (Source && Source->GetScalarParameterValue(FHashedMaterialParameterInfo(MetaHumanSweat::Param::Amount), Unused))
			{
				FMetaHumanSweatBinding& Adopted = Bindings.AddDefaulted_GetRef();
				Adopted.Component = Component;
				Adopted.Mesh = Component->GetSkeletalMeshAsset();
				Adopted.Material = Orphan;
				Adopted.OriginalOverride = nullptr;
				Adopted.Slot = Slot;
				continue;
			}
		}
		UMaterialInterface* Parent = ResolveSweatMaterial(Source);
		if (!Parent)
		{
			continue;
		}
		++SkinSlotsSeen;
		// The transient package owns the instance: one owned by the component can keep a play world alive through the renderer.
		UMaterialInstanceDynamic* Dynamic = UMaterialInstanceDynamic::Create(Parent, GetTransientPackage());
		if (!Dynamic)
		{
			continue;
		}
		if (UMaterialInstanceDynamic* Live = Cast<UMaterialInstanceDynamic>(Current))
		{
			// The slot already held a dynamic instance (a Blueprint tinting the skin, for example): carry its values over.
			Dynamic->CopyParameterOverrides(Live);
		}
		FMetaHumanSweatBinding& Binding = Bindings.AddDefaulted_GetRef();
		Binding.Component = Component;
		Binding.Mesh = Component->GetSkeletalMeshAsset();
		Binding.Material = Dynamic;
		Binding.OriginalOverride = Component->OverrideMaterials.IsValidIndex(Slot) ? Component->OverrideMaterials[Slot].Get() : nullptr;
		Binding.Slot = Slot;
		Component->SetMaterial(Slot, Dynamic);
		UE_LOGF(LogMetaHumanSweat, Log, "%ls: slot %d of %ls now draws %ls (was %ls)", *GetNameSafe(GetOwner()), Slot, *Component->GetName(), *Parent->GetName(), *GetNameSafe(Current));
	}
}

void UMetaHumanSweatComponent::BindSlots()
{
	bLoadsPending = false;
	SkinSlotsSeen = 0;
	// Drop what is no longer ours: the mesh changed, or something else put its own material in the slot (theirs is left alone).
	for (int32 Index = Bindings.Num() - 1; Index >= 0; --Index)
	{
		const FMetaHumanSweatBinding& Binding = Bindings[Index];
		const bool bOurs = IsValid(Binding.Component) && Binding.Material && Binding.Component->GetSkeletalMeshAsset() == Binding.Mesh
			&& Binding.Component->GetMaterial(Binding.Slot) == Binding.Material;
		if (!bOurs)
		{
			UE_LOGF(LogMetaHumanSweat, Log, "%ls: slot %d of %ls was taken by %ls; it is left alone", *GetNameSafe(GetOwner()), Binding.Slot, *GetNameSafe(Binding.Component),
				IsValid(Binding.Component) ? *GetNameSafe(Binding.Component->GetMaterial(Binding.Slot)) : TEXT("nothing (the component is gone)"));
			Bindings.RemoveAtSwap(Index);
		}
	}
	SkinSlotsSeen = Bindings.Num();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	TInlineComponentArray<USkeletalMeshComponent*> Components(Owner);
	for (USkeletalMeshComponent* Component : Components)
	{
		BindComponent(Component);
	}
	if (!Bindings.IsEmpty())
	{
		Note.Reset();
	}
	else if (bLoadsPending)
	{
		Note = TEXT("loading the sweat materials");
	}
	else if (Components.IsEmpty())
	{
		Note = TEXT("the actor has no skeletal mesh component");
	}
	else
	{
		Note = TEXT("no sweat materials for this character's skin: select it and run Tools > MetaHuman Sweat > Add Sweat to Selected Actors");
	}
}

void UMetaHumanSweatComponent::PushParameters()
{
	if (Bindings.IsEmpty())
	{
		return;
	}
	const FMetaHumanSweatParams Params = MetaHumanSweat::MapParameters(Model, ActiveLook(), Seed);
	for (FMetaHumanSweatBinding& Binding : Bindings)
	{
		if (Binding.Material && (!Binding.bApplied || !Binding.Last.NearlyEquals(Params)))
		{
			MetaHumanSweat::ApplyParameters(Binding.Material, Params);
			Binding.Last = Params;
			Binding.bApplied = true;
		}
	}
}

void UMetaHumanSweatComponent::ReleaseAll()
{
	for (FMetaHumanSweatBinding& Binding : Bindings)
	{
		if (IsValid(Binding.Component) && Binding.Material && Binding.Slot >= 0 && Binding.Component->GetMaterial(Binding.Slot) == Binding.Material)
		{
			Binding.Component->SetMaterial(Binding.Slot, Binding.OriginalOverride);
			UE_LOGF(LogMetaHumanSweat, Log, "%ls: slot %d of %ls restored to %ls", *GetNameSafe(GetOwner()), Binding.Slot, *Binding.Component->GetName(), *GetNameSafe(Binding.Component->GetMaterial(Binding.Slot)));
		}
	}
	Bindings.Reset();
}
