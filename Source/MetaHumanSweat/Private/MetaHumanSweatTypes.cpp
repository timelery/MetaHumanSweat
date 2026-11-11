#include "MetaHumanSweatTypes.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Crc.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanSweatTypes)

DEFINE_LOG_CATEGORY(LogMetaHumanSweat);

namespace MetaHumanSweatPrivate
{
	float FiniteClamp(float Value, float Min, float Max, float Fallback)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Fallback;
	}

	double UnitInput(double Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0, 1.0) : 0.0;
	}

	// dx/dt = Production * (1 - x) - Removal * x, in closed form.
	double Respond(double Current, double Production, double Removal, double Seconds)
	{
		const double Rate = Production + Removal;
		if (Rate <= 0.0)
		{
			return FMath::Clamp(Current, 0.0, 1.0);
		}
		const double Equilibrium = Production / Rate;
		return FMath::Clamp(Equilibrium + (Current - Equilibrium) * FMath::Exp(-Rate * Seconds), 0.0, 1.0);
	}

	constexpr double MaxStepSeconds = 60.0;
	constexpr int32 MaxNameLength = 40;
}

FMetaHumanSweatLook FMetaHumanSweatLook::Clamped() const
{
	using namespace MetaHumanSweatPrivate;
	FMetaHumanSweatLook Result;
	Result.Intensity = FiniteClamp(Intensity, 0.f, 2.f, 1.f);
	Result.BeadDensity = FiniteClamp(BeadDensity, 0.25f, 4.f, 1.f);
	Result.BeadSize = FiniteClamp(BeadSize, 0.25f, 3.f, 1.f);
	Result.StreakLength = FiniteClamp(StreakLength, 0.f, 3.f, 1.f);
	Result.FlowFlip = FMath::Clamp(FlowFlip, 0, 3);
	return Result;
}

FMetaHumanSweatRates FMetaHumanSweatRates::Clamped() const
{
	using namespace MetaHumanSweatPrivate;
	FMetaHumanSweatRates Result;
	Result.SweatBuildRate = FiniteClamp(SweatBuildRate, 0.005f, 0.5f, 0.04f);
	Result.SweatDryRate = FiniteClamp(SweatDryRate, 0.002f, 0.2f, 0.015f);
	Result.RainRate = FiniteClamp(RainRate, 0.005f, 1.f, 0.08f);
	Result.ImmersionRate = FiniteClamp(ImmersionRate, 0.05f, 5.f, 0.8f);
	Result.WetnessDryRate = FiniteClamp(WetnessDryRate, 0.001f, 0.2f, 0.01f);
	return Result;
}

void FMetaHumanSweatModel::Advance(double Seconds, double Effort, double RainExposure, double Immersion, const FMetaHumanSweatRates& InRates)
{
	using namespace MetaHumanSweatPrivate;
	if (!FMath::IsFinite(Seconds) || Seconds <= 0.0)
	{
		return;
	}
	const FMetaHumanSweatRates Rates = InRates.Clamped();
	const double Step = FMath::Min(Seconds, MaxStepSeconds);
	Wetness = Respond(Wetness, UnitInput(RainExposure) * Rates.RainRate + UnitInput(Immersion) * Rates.ImmersionRate, Rates.WetnessDryRate, Step);
	Sweat = Respond(Sweat, UnitInput(Effort) * Rates.SweatBuildRate, Rates.SweatDryRate, Step);
}

bool FMetaHumanSweatParams::NearlyEquals(const FMetaHumanSweatParams& Other, float Tolerance) const
{
	return FMath::IsNearlyEqual(Amount, Other.Amount, Tolerance)
		&& FMath::IsNearlyEqual(Wetness, Other.Wetness, Tolerance)
		&& FMath::IsNearlyEqual(Intensity, Other.Intensity, Tolerance)
		&& FMath::IsNearlyEqual(Density, Other.Density, Tolerance)
		&& FMath::IsNearlyEqual(BeadSize, Other.BeadSize, Tolerance)
		&& FMath::IsNearlyEqual(StreakLength, Other.StreakLength, Tolerance)
		&& FMath::IsNearlyEqual(FlowAxes.R, Other.FlowAxes.R, Tolerance)
		&& FMath::IsNearlyEqual(FlowAxes.G, Other.FlowAxes.G, Tolerance)
		&& FMath::IsNearlyEqual(Seed, Other.Seed, Tolerance);
}

namespace MetaHumanSweat
{
	FMetaHumanSweatParams MapParameters(const FMetaHumanSweatModel& Model, const FMetaHumanSweatLook& Look, float Seed)
	{
		using namespace MetaHumanSweatPrivate;
		const FMetaHumanSweatLook Clamped = Look.Clamped();
		FMetaHumanSweatParams Params;
		Params.Amount = static_cast<float>(UnitInput(Model.Sweat));
		Params.Wetness = static_cast<float>(UnitInput(Model.Wetness));
		Params.Intensity = Clamped.Intensity;
		// The shader scales cells per UV, so a count per area goes in as its square root.
		Params.Density = FMath::Sqrt(Clamped.BeadDensity);
		Params.BeadSize = Clamped.BeadSize;
		Params.StreakLength = Clamped.StreakLength;
		Params.FlowAxes = FLinearColor((Clamped.FlowFlip & 1) != 0 ? -1.f : 1.f, (Clamped.FlowFlip & 2) != 0 ? -1.f : 1.f, 0.f, 0.f);
		Params.Seed = FiniteClamp(Seed, 0.f, 1.f, 0.f);
		return Params;
	}

	void ApplyParameters(UMaterialInstanceDynamic* Material, const FMetaHumanSweatParams& Params)
	{
		if (!Material)
		{
			return;
		}
		Material->SetScalarParameterValue(Param::Amount, Params.Amount);
		Material->SetScalarParameterValue(Param::Wetness, Params.Wetness);
		Material->SetScalarParameterValue(Param::Intensity, Params.Intensity);
		Material->SetScalarParameterValue(Param::Density, Params.Density);
		Material->SetScalarParameterValue(Param::BeadSize, Params.BeadSize);
		Material->SetScalarParameterValue(Param::StreakLength, Params.StreakLength);
		Material->SetScalarParameterValue(Param::Seed, Params.Seed);
		Material->SetVectorParameterValue(Param::FlowAxes, Params.FlowAxes);
	}

	float SeedFromName(const FString& Name)
	{
		const uint32 Crc = FCrc::StrCrc32(*Name);
		return static_cast<float>(Crc & 0xFFFFFFu) / 16777216.f;
	}

	uint32 PathHash(const FString& Text)
	{
		const FString Lower = Text.ToLower();
		const FTCHARToUTF8 Utf8(*Lower);
		const uint8* Bytes = reinterpret_cast<const uint8*>(Utf8.Get());
		uint32 Hash = 2166136261u;
		for (int32 Index = 0; Index < Utf8.Length(); ++Index)
		{
			Hash ^= static_cast<uint32>(Bytes[Index]);
			Hash *= 16777619u;
		}
		return Hash;
	}

	FString SweatAssetName(const TCHAR* Prefix, const FString& SourcePackageName, const FString& SourceAssetName)
	{
		FString Name;
		Name.Reserve(MetaHumanSweatPrivate::MaxNameLength);
		for (int32 Index = 0; Index < SourceAssetName.Len() && Name.Len() < MetaHumanSweatPrivate::MaxNameLength; ++Index)
		{
			const TCHAR Character = SourceAssetName[Index];
			const bool bSafe = (Character >= TEXT('0') && Character <= TEXT('9')) || (Character >= TEXT('A') && Character <= TEXT('Z'))
				|| (Character >= TEXT('a') && Character <= TEXT('z')) || Character == TEXT('_');
			Name.AppendChar(bSafe ? Character : TEXT('_'));
		}
		return FString::Printf(TEXT("%s_%s_%08X"), Prefix, *Name, PathHash(SourcePackageName));
	}

	FString SweatMaterialObjectPath(const FString& Folder, const FString& DryPackageName, const FString& DryAssetName)
	{
		const FString Leaf = SweatAssetName(TEXT("MI_Sweat"), DryPackageName, DryAssetName);
		return FString::Printf(TEXT("%s/Materials/%s.%s"), *Folder, *Leaf, *Leaf);
	}
}
