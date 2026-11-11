#include "MetaHumanSweatComponent.h"
#include "MetaHumanSweatTypes.h"

#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectIterator.h"

namespace MetaHumanSweatConsole
{
	TAutoConsoleVariable<int32> CVarEnabled(TEXT("mhs.Enabled"), 1,
		TEXT("MetaHuman Sweat master switch. 1 (default): every MetaHuman Sweat component runs. 0: all of them restore the original skin materials and stop."),
		ECVF_Default);
	TAutoConsoleVariable<int32> CVarDebug(TEXT("mhs.Debug"), 0,
		TEXT("1 prints one on-screen line per MetaHuman Sweat component: sweat and wetness levels, effort, how many skin slots are bound and, when none is, why. 0 (default) prints nothing."),
		ECVF_Default);

	template <typename FunctionType>
	void ForEachComponent(UWorld* World, FunctionType&& Function)
	{
		int32 Count = 0;
		for (TObjectIterator<UMetaHumanSweatComponent> It; It; ++It)
		{
			if (It->GetWorld() == World && It->HasBegunPlay())
			{
				Function(**It);
				++Count;
			}
		}
		UE_LOGF(LogMetaHumanSweat, Log, "%d MetaHuman Sweat component(s) in this world", Count);
	}

	float UnitArgument(const TArray<FString>& Args, float Fallback)
	{
		return Args.Num() > 0 ? FMath::Clamp(FCString::Atof(*Args[0]), 0.f, 1.f) : Fallback;
	}

	FAutoConsoleCommandWithWorldAndArgs ExertCommand(TEXT("mhs.Exert"),
		TEXT("mhs.Exert <0-1>: sets Effort on every MetaHuman Sweat component in the world (1 when no value is given). Sweat then builds by itself; mhs.Exert 0 lets them dry."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Value = UnitArgument(Args, 1.f);
			ForEachComponent(World, [Value](UMetaHumanSweatComponent& Component) { Component.SetEffort(Value); });
		}));

	FAutoConsoleCommandWithWorldAndArgs SetSweatCommand(TEXT("mhs.SetSweat"),
		TEXT("mhs.SetSweat <0-1>: jumps the sweat level of every MetaHuman Sweat component in the world to a value (0.7 when none is given), to look at a level without waiting."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Value = UnitArgument(Args, 0.7f);
			ForEachComponent(World, [Value](UMetaHumanSweatComponent& Component) { Component.SetSweatLevel(Value); });
		}));

	FAutoConsoleCommandWithWorldAndArgs SetWetnessCommand(TEXT("mhs.SetWetness"),
		TEXT("mhs.SetWetness <0-1>: jumps the rain and immersion wetness of every MetaHuman Sweat component in the world to a value (0.7 when none is given)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			const float Value = UnitArgument(Args, 0.7f);
			ForEachComponent(World, [Value](UMetaHumanSweatComponent& Component) { Component.SetWetnessLevel(Value); });
		}));

	FAutoConsoleCommandWithWorldAndArgs DryCommand(TEXT("mhs.Dry"),
		TEXT("mhs.Dry: sets sweat and wetness to 0 and Effort to 0 on every MetaHuman Sweat component in the world."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachComponent(World, [](UMetaHumanSweatComponent& Component) { Component.SetEffort(0.f); Component.DryNow(); });
		}));

	FAutoConsoleCommandWithWorldAndArgs StatusCommand(TEXT("mhs.Status"),
		TEXT("mhs.Status: logs one line per MetaHuman Sweat component in the world (levels, bound skin slots and, when none is bound, why)."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			ForEachComponent(World, [](UMetaHumanSweatComponent& Component) { UE_LOGF(LogMetaHumanSweat, Log, "MetaHumanSweat status: %ls", *Component.GetSweatStatus()); });
		}));
}

IMPLEMENT_MODULE(FDefaultModuleImpl, MetaHumanSweat)
