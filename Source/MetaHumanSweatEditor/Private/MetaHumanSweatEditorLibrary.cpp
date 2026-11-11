#include "MetaHumanSweatEditorLibrary.h"

#include "MetaHumanSweatComponent.h"
#include "MetaHumanSweatSettings.h"
#include "MetaHumanSweatTypes.h"

#include "AssetImportTask.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "AssetToolsModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Interfaces/IPluginManager.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionBreakMaterialAttributes.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionSetMaterialAttributes.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionTransform.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshLODModel.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Settings/ProjectPackagingSettings.h"
#include "UObject/Package.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanSweatEditorLibrary)

namespace MetaHumanSweatAuthoring
{
	/** Every node of the sweat block carries this prefix in its description, so the block can be found and rebuilt. */
	const TCHAR* const NodeTag = TEXT("MetaHumanSweat/");
	const TCHAR* const ParameterGroup = TEXT("MetaHuman Sweat");
	/** Used when a mesh carries no geometry to measure: cm of skin per UV unit on a MetaHuman face and body. */
	constexpr double DefaultFaceCmPerUV = 49.7;
	constexpr double DefaultBodyCmPerUV = 155.6;

	struct FReport
	{
		TArray<FString> Lines;
		int32 Created = 0;
		int32 Problems = 0;

		void Say(const FString& Line)
		{
			Lines.Add(Line);
			UE_LOGF(LogMetaHumanSweat, Log, "%ls", *Line);
		}

		void Problem(const FString& Line)
		{
			++Problems;
			Lines.Add(TEXT("PROBLEM: ") + Line);
			UE_LOGF(LogMetaHumanSweat, Warning, "%ls", *Line);
		}

		FString Text() const
		{
			return FString::Join(Lines, TEXT("\n"));
		}
	};

	/** Everything one call shares. */
	struct FContext
	{
		FReport Report;
		TArray<UPackage*> ToSave;
		TSet<FString> DoneLeaves;
		FString Folder;
		int32 SkinSlots = 0;

		void Touch(UObject* Object)
		{
			if (Object)
			{
				Object->MarkPackageDirty();
				ToSave.AddUnique(Object->GetOutermost());
			}
		}
	};

	FString ResourcePath(const FString& Relative)
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("MetaHumanSweat"));
		return Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / TEXT("Resources") / Relative) : FString();
	}

	bool LoadShader(const TCHAR* Name, FString& OutCode, FReport& Report)
	{
		const FString File = ResourcePath(FString(TEXT("Shaders")) / Name);
		if (!FFileHelper::LoadFileToString(OutCode, *File) || OutCode.IsEmpty())
		{
			Report.Problem(FString::Printf(TEXT("Cannot read the shader file %s (is the plugin's Resources folder complete?)."), *File));
			return false;
		}
		OutCode.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		return true;
	}

	template <typename AssetType>
	AssetType* LoadIfExists(const FString& PackageName)
	{
		const FString ObjectPath = PackageName + TEXT(".") + FPackageName::GetShortName(PackageName);
		if (AssetType* Found = FindObject<AssetType>(nullptr, *ObjectPath))
		{
			return Found;
		}
		return FPackageName::DoesPackageExist(PackageName) ? LoadObject<AssetType>(nullptr, *ObjectPath) : nullptr;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Masks

	UTexture2D* EnsureMask(FContext& Context, bool bBody)
	{
		const FString Name = bBody ? TEXT("T_MHSweat_BodyMask") : TEXT("T_MHSweat_FaceMask");
		const FString Destination = Context.Folder / TEXT("Textures");
		const FString PackageName = Destination / Name;
		if (UTexture2D* Existing = LoadIfExists<UTexture2D>(PackageName))
		{
			return Existing;
		}
		const FString File = ResourcePath(FString(TEXT("Masks")) / (bBody ? TEXT("BodyMask.png") : TEXT("FaceMask.png")));
		if (!FPaths::FileExists(File))
		{
			Context.Report.Problem(FString::Printf(TEXT("The mask file %s is missing from the plugin."), *File));
			return nullptr;
		}
		UAssetImportTask* Task = NewObject<UAssetImportTask>();
		Task->Filename = File;
		Task->DestinationPath = Destination;
		Task->DestinationName = Name;
		Task->bAutomated = true;
		Task->bReplaceExisting = false;
		Task->bSave = false;
		UTextureFactory* Factory = NewObject<UTextureFactory>();
		Factory->SuppressImportOverwriteDialog();
		Task->Factory = Factory;
		FAssetToolsModule::GetModule().Get().ImportAssetTasks({ Task });
		UTexture2D* Texture = FindObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + Name));
		if (!Texture)
		{
			Context.Report.Problem(FString::Printf(TEXT("Importing %s into %s did not produce a texture."), *File, *PackageName));
			return nullptr;
		}
		// A linear data mask: the material samples it with the Masks sampler type.
		Texture->PreEditChange(nullptr);
		Texture->SRGB = false;
		Texture->CompressionSettings = TC_Masks;
		Texture->PostEditChange();
		Context.Touch(Texture);
		++Context.Report.Created;
		Context.Report.Say(FString::Printf(TEXT("Imported the %s mask: %s"), bBody ? TEXT("body") : TEXT("face"), *PackageName));
		return Texture;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The sweat block of a master material

	template <typename ExpressionType>
	ExpressionType* NewNode(UMaterial* Material, const TCHAR* Key, int32 X, int32 Y)
	{
		ExpressionType* Node = Cast<ExpressionType>(UMaterialEditingLibrary::CreateMaterialExpression(Material, ExpressionType::StaticClass(), X, Y));
		if (Node)
		{
			Node->Desc = FString(NodeTag) + Key;
		}
		return Node;
	}

	UMaterialExpressionScalarParameter* ScalarNode(UMaterial* Material, const FName& Name, float Default, int32 X, int32 Y)
	{
		UMaterialExpressionScalarParameter* Node = NewNode<UMaterialExpressionScalarParameter>(Material, *(FString(TEXT("S.")) + Name.ToString()), X, Y);
		if (Node)
		{
			Node->ParameterName = Name;
			Node->DefaultValue = Default;
			Node->Group = ParameterGroup;
		}
		return Node;
	}

	UMaterialExpressionVectorParameter* VectorNode(UMaterial* Material, const FName& Name, const FLinearColor& Default, int32 X, int32 Y)
	{
		UMaterialExpressionVectorParameter* Node = NewNode<UMaterialExpressionVectorParameter>(Material, *(FString(TEXT("V.")) + Name.ToString()), X, Y);
		if (Node)
		{
			Node->ParameterName = Name;
			Node->DefaultValue = Default;
			Node->Group = ParameterGroup;
		}
		return Node;
	}

	UMaterialExpressionCustom* CustomNode(UMaterial* Material, const TCHAR* Key, int32 X, int32 Y, const FString& Code, ECustomMaterialOutputType OutputType, std::initializer_list<const TCHAR*> InputNames)
	{
		UMaterialExpressionCustom* Node = NewNode<UMaterialExpressionCustom>(Material, Key, X, Y);
		if (Node)
		{
			Node->Code = Code;
			Node->OutputType = OutputType;
			Node->Description = FString(TEXT("MetaHuman Sweat ")) + Key;
			Node->Inputs.Reset();
			for (const TCHAR* InputName : InputNames)
			{
				FCustomInput Input;
				Input.InputName = InputName;
				Node->Inputs.Add(Input);
			}
		}
		return Node;
	}

	int32 OutputIndexByName(UMaterialExpression* Node, const TCHAR* Name)
	{
		const TArray<FExpressionOutput>& Outputs = Node->GetOutputs();
		for (int32 Index = 0; Index < Outputs.Num(); ++Index)
		{
			if (Outputs[Index].OutputName == FName(Name))
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool HasBlock(const UMaterial* Material)
	{
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (Expression && Expression->Desc.StartsWith(NodeTag))
			{
				return true;
			}
		}
		return false;
	}

	/** Takes the sweat block out again and reconnects the output to what fed the block. False if the block is not intact. */
	bool RemoveBlock(UMaterial* Material, FReport& Report)
	{
		TArray<UMaterialExpression*> Tagged;
		UMaterialExpressionSetMaterialAttributes* Set = nullptr;
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (Expression && Expression->Desc.StartsWith(NodeTag))
			{
				Tagged.Add(Expression);
				if (Expression->Desc == FString(NodeTag) + TEXT("Set"))
				{
					Set = Cast<UMaterialExpressionSetMaterialAttributes>(Expression);
				}
			}
		}
		if (Tagged.IsEmpty())
		{
			return true;
		}
		if (!Set || Set->Inputs.IsEmpty() || !Set->Inputs[0].Expression || Tagged.Contains(Set->Inputs[0].Expression))
		{
			Report.Problem(FString::Printf(TEXT("%s: the sweat block was edited by hand and cannot be rebuilt. Delete this material and its MI_Sweat instances, then add sweat again."), *Material->GetPathName()));
			return false;
		}
		UMaterialExpression* Original = Set->Inputs[0].Expression;
		const int32 OriginalOutput = Set->Inputs[0].OutputIndex;
		Material->GetEditorOnlyData()->MaterialAttributes.Connect(OriginalOutput, Original);
		for (UMaterialExpression* Expression : Tagged)
		{
			UMaterialEditingLibrary::DeleteMaterialExpression(Material, Expression);
		}
		return true;
	}

	/** Adds the sweat block in front of the Material Attributes output. The caller recompiles. */
	bool BuildBlock(UMaterial* Material, UTexture2D* DefaultMask, FReport& Report)
	{
		if (!Material->bUseMaterialAttributes)
		{
			Report.Problem(FString::Printf(TEXT("%s does not use Material Attributes; this version supports only skin masters that do (the MetaHuman Creator skin, M_skin_unified_baked)."), *Material->GetPathName()));
			return false;
		}
		FExpressionInput& Root = Material->GetEditorOnlyData()->MaterialAttributes;
		UMaterialExpression* RootExpression = Root.Expression;
		const int32 RootOutput = Root.OutputIndex;
		if (!RootExpression)
		{
			Report.Problem(FString::Printf(TEXT("%s has nothing connected to its Material Attributes output."), *Material->GetPathName()));
			return false;
		}
		FString FieldCode, BaseColorCode, RoughnessCode, SpecularCode, NormalCode;
		if (!LoadShader(TEXT("Field.hlsl"), FieldCode, Report) || !LoadShader(TEXT("BlendBaseColor.hlsl"), BaseColorCode, Report) || !LoadShader(TEXT("BlendRoughness.hlsl"), RoughnessCode, Report)
			|| !LoadShader(TEXT("BlendSpecular.hlsl"), SpecularCode, Report) || !LoadShader(TEXT("BlendNormal.hlsl"), NormalCode, Report))
		{
			return false;
		}

		// To the right of what fed the output, so the block reads left to right in the material editor.
		const int32 X = RootExpression->MaterialExpressionEditorX + 500;
		const int32 Y = RootExpression->MaterialExpressionEditorY;

		UMaterialExpressionBreakMaterialAttributes* Break = NewNode<UMaterialExpressionBreakMaterialAttributes>(Material, TEXT("Break"), X, Y);
		UMaterialExpressionSetMaterialAttributes* Set = NewNode<UMaterialExpressionSetMaterialAttributes>(Material, TEXT("Set"), X + 1700, Y);
		UMaterialExpressionTextureCoordinate* UV = NewNode<UMaterialExpressionTextureCoordinate>(Material, TEXT("UV0"), X, Y + 600);
		UMaterialExpressionTextureSampleParameter2D* Mask = NewNode<UMaterialExpressionTextureSampleParameter2D>(Material, TEXT("Mask"), X + 250, Y + 600);
		UMaterialExpressionTransform* UpTangent = NewNode<UMaterialExpressionTransform>(Material, TEXT("UpTangent"), X + 500, Y + 2100);
		UMaterialExpressionTime* Time = NewNode<UMaterialExpressionTime>(Material, TEXT("Time"), X + 250, Y + 2500);
		if (!Break || !Set || !UV || !Mask || !UpTangent || !Time || Set->Inputs.IsEmpty())
		{
			Report.Problem(FString::Printf(TEXT("%s: could not create the material nodes."), *Material->GetPathName()));
			return false;
		}
		Break->MaterialAttributes.Connect(RootOutput, RootExpression);
		Set->Inputs[0].Connect(RootOutput, RootExpression);

		UV->CoordinateIndex = 0;
		Mask->ParameterName = MetaHumanSweat::Param::Mask;
		Mask->Group = ParameterGroup;
		Mask->Texture = DefaultMask;
		Mask->SamplerType = SAMPLERTYPE_Masks;
		Mask->SamplerSource = SSM_Wrap_WorldGroupSettings;
		Mask->Coordinates.Connect(0, UV);

		using namespace MetaHumanSweat;
		UMaterialExpressionScalarParameter* Amount = ScalarNode(Material, Param::Amount, 0.f, X + 250, Y + 900);
		UMaterialExpressionScalarParameter* Wetness = ScalarNode(Material, Param::Wetness, 0.f, X + 250, Y + 1030);
		UMaterialExpressionScalarParameter* Intensity = ScalarNode(Material, Param::Intensity, 1.f, X + 250, Y + 1160);
		UMaterialExpressionScalarParameter* Density = ScalarNode(Material, Param::Density, 1.f, X + 250, Y + 1290);
		UMaterialExpressionScalarParameter* BeadSize = ScalarNode(Material, Param::BeadSize, 1.f, X + 250, Y + 1420);
		UMaterialExpressionScalarParameter* StreakLength = ScalarNode(Material, Param::StreakLength, 1.f, X + 250, Y + 1550);
		UMaterialExpressionScalarParameter* Seed = ScalarNode(Material, Param::Seed, 0.f, X + 250, Y + 1680);
		UMaterialExpressionScalarParameter* Cells = ScalarNode(Material, Param::Cells, 100.f, X + 250, Y + 1810);
		UMaterialExpressionVectorParameter* FlowAxes = VectorNode(Material, Param::FlowAxes, FLinearColor(1.f, 1.f, 0.f, 0.f), X + 250, Y + 1940);
		UMaterialExpressionVectorParameter* WorldUp = VectorNode(Material, Param::WorldUp, FLinearColor(0.f, 0.f, 1.f, 0.f), X + 250, Y + 2140);
		if (!Amount || !Wetness || !Intensity || !Density || !BeadSize || !StreakLength || !Seed || !Cells || !FlowAxes || !WorldUp)
		{
			Report.Problem(FString::Printf(TEXT("%s: could not create the parameter nodes."), *Material->GetPathName()));
			return false;
		}
		// Gravity for the running drops: world up, turned into the surface's tangent frame at every pixel.
		UpTangent->TransformSourceType = TRANSFORMSOURCE_World;
		UpTangent->TransformType = TRANSFORM_Tangent;
		UpTangent->Input.Connect(0, WorldUp);

		UMaterialExpressionCustom* Field = CustomNode(Material, TEXT("Field"), X + 800, Y + 900, FieldCode, CMOT_Float4,
			{ TEXT("UV"), TEXT("Region"), TEXT("Up"), TEXT("Amount"), TEXT("Wet"), TEXT("Cells"), TEXT("Density"), TEXT("Size"), TEXT("Streak"), TEXT("Intensity"), TEXT("Axes"), TEXT("Seed"), TEXT("GameTime") });
		if (!Field)
		{
			return false;
		}
		const int32 MaskRGBA = 5;  // Texture sample outputs: RGB, R, G, B, A, RGBA
		Field->Inputs[0].Input.Connect(0, UV);
		Field->Inputs[1].Input.Connect(MaskRGBA, Mask);
		Field->Inputs[2].Input.Connect(0, UpTangent);
		Field->Inputs[3].Input.Connect(0, Amount);
		Field->Inputs[4].Input.Connect(0, Wetness);
		Field->Inputs[5].Input.Connect(0, Cells);
		Field->Inputs[6].Input.Connect(0, Density);
		Field->Inputs[7].Input.Connect(0, BeadSize);
		Field->Inputs[8].Input.Connect(0, StreakLength);
		Field->Inputs[9].Input.Connect(0, Intensity);
		Field->Inputs[10].Input.Connect(0, FlowAxes);
		Field->Inputs[11].Input.Connect(0, Seed);
		Field->Inputs[12].Input.Connect(0, Time);

		struct FBlend
		{
			const TCHAR* Key;
			const TCHAR* Attribute;
			const TCHAR* InputName;
			const FString* Code;
			ECustomMaterialOutputType OutputType;
			EMaterialProperty Property;
		};
		const FBlend Blends[] = {
			{ TEXT("BlendBaseColor"), TEXT("BaseColor"), TEXT("Base"), &BaseColorCode, CMOT_Float3, MP_BaseColor },
			{ TEXT("BlendRoughness"), TEXT("Roughness"), TEXT("Rough"), &RoughnessCode, CMOT_Float1, MP_Roughness },
			{ TEXT("BlendSpecular"), TEXT("Specular"), TEXT("Spec"), &SpecularCode, CMOT_Float1, MP_Specular },
			{ TEXT("BlendNormal"), TEXT("Normal"), TEXT("N"), &NormalCode, CMOT_Float3, MP_Normal },
		};
		int32 Row = 0;
		for (const FBlend& Item : Blends)
		{
			const int32 BreakOutput = OutputIndexByName(Break, Item.Attribute);
			UMaterialExpressionCustom* Blend = CustomNode(Material, Item.Key, X + 1250, Y + Row * 260, *Item.Code, Item.OutputType, { Item.InputName, TEXT("Field") });
			if (!Blend || BreakOutput == INDEX_NONE)
			{
				Report.Problem(FString::Printf(TEXT("%s: could not wire the %s blend."), *Material->GetPathName(), Item.Attribute));
				return false;
			}
			Blend->Inputs[0].Input.Connect(BreakOutput, Break);
			Blend->Inputs[1].Input.Connect(0, Field);
			if (!Set->ConnectInputAttribute(Item.Property, Blend, 0))
			{
				Report.Problem(FString::Printf(TEXT("%s: could not connect %s to the Set Material Attributes node."), *Material->GetPathName(), Item.Attribute));
				return false;
			}
			++Row;
		}
		Root.Connect(0, Set);
		return true;
	}

	bool Recompile(UMaterial* Material, FReport& Report)
	{
		Material->PreEditChange(nullptr);
		Material->PostEditChange();
		const TArray<FString> Errors = UMaterialEditingLibrary::RecompileMaterial(Material);
		if (!Errors.IsEmpty())
		{
			Report.Problem(FString::Printf(TEXT("%s did not compile: %s"), *Material->GetPathName(), *FString::Join(Errors, TEXT(" | "))));
			return false;
		}
		return true;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Materials of a skin slot

	/** Instances from the given material up to (not including) the base material; false unless every one is a material instance asset. */
	bool GetChain(UMaterialInterface* Material, TArray<UMaterialInstanceConstant*>& OutInstances, UMaterial*& OutBase)
	{
		OutInstances.Reset();
		OutBase = nullptr;
		UMaterialInterface* Current = Material;
		for (int32 Depth = 0; Current && Depth < 32; ++Depth)
		{
			if (UMaterial* Base = Cast<UMaterial>(Current))
			{
				OutBase = Base;
				return true;
			}
			UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(Current);
			if (!Instance)
			{
				return false;
			}
			OutInstances.Add(Instance);
			Current = Instance->Parent;
		}
		return false;
	}

	bool IsSkinMaster(const UMaterial* Base)
	{
		for (const FString& Name : UMetaHumanSweatSettings::Get().SkinMaterialNames)
		{
			if (Base->GetName().Equals(Name.TrimStartAndEnd(), ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	UMaterial* EnsureMaster(FContext& Context, UMaterial* Base, UTexture2D* DefaultMask)
	{
		const FString Name = MetaHumanSweat::SweatAssetName(TEXT("M_Sweat"), Base->GetOutermost()->GetName(), Base->GetName());
		const FString PackagePath = Context.Folder / TEXT("Materials");
		if (UMaterial* Existing = LoadIfExists<UMaterial>(PackagePath / Name))
		{
			if (HasBlock(Existing))
			{
				return Existing;
			}
			Context.Report.Say(FString::Printf(TEXT("%s exists without its sweat block: rebuilding it."), *Existing->GetPathName()));
			if (!BuildBlock(Existing, DefaultMask, Context.Report) || !Recompile(Existing, Context.Report))
			{
				return nullptr;
			}
			Context.Touch(Existing);
			return Existing;
		}
		UMaterial* Copy = Cast<UMaterial>(FAssetToolsModule::GetModule().Get().DuplicateAsset(Name, PackagePath, Base));
		if (!Copy)
		{
			Context.Report.Problem(FString::Printf(TEXT("Could not copy %s to %s/%s."), *Base->GetPathName(), *PackagePath, *Name));
			return nullptr;
		}
		if (!BuildBlock(Copy, DefaultMask, Context.Report) || !Recompile(Copy, Context.Report))
		{
			return nullptr;
		}
		Context.Touch(Copy);
		++Context.Report.Created;
		Context.Report.Say(FString::Printf(TEXT("Created the sweat master %s from %s"), *Copy->GetPathName(), *Base->GetPathName()));
		return Copy;
	}

	struct FSlotMeasure
	{
		bool bMeasured = false;
		bool bBody = false;
		double CmPerUV = 0.0;
		int32 Triangles = 0;
	};

	/** Which UV tile the slot's triangles use (a MetaHuman body sits in U 1..2, a face in 0..1) and how many cm of surface one UV unit covers. */
	FSlotMeasure MeasureSlot(const USkeletalMesh* Mesh, int32 MaterialIndex)
	{
		FSlotMeasure Result;
		const FSkeletalMeshModel* Model = Mesh ? Mesh->GetImportedModel() : nullptr;
		if (!Model)
		{
			return Result;
		}
		for (int32 LODIndex = 0; LODIndex < Model->LODModels.Num() && !Result.bMeasured; ++LODIndex)
		{
			const FSkeletalMeshLODModel& LOD = Model->LODModels[LODIndex];
			const FSkeletalMeshLODInfo* Info = Mesh->GetLODInfo(LODIndex);
			TArray<TPair<double, double>> Scales;  // (cm per UV, surface area) per triangle
			double SumU = 0.0;
			int64 CountU = 0;
			for (int32 SectionIndex = 0; SectionIndex < LOD.Sections.Num(); ++SectionIndex)
			{
				const FSkelMeshSection& Section = LOD.Sections[SectionIndex];
				int32 Material = Section.MaterialIndex;
				if (Info && Info->LODMaterialMap.IsValidIndex(SectionIndex) && Info->LODMaterialMap[SectionIndex] != INDEX_NONE)
				{
					Material = Info->LODMaterialMap[SectionIndex];
				}
				if (Material != MaterialIndex || Section.SoftVertices.IsEmpty())
				{
					continue;
				}
				for (uint32 Triangle = 0; Triangle < Section.NumTriangles; ++Triangle)
				{
					const uint32 First = Section.BaseIndex + Triangle * 3;
					if (!LOD.IndexBuffer.IsValidIndex(static_cast<int32>(First + 2)))
					{
						break;
					}
					const int32 A = static_cast<int32>(static_cast<int64>(LOD.IndexBuffer[First]) - static_cast<int64>(Section.BaseVertexIndex));
					const int32 B = static_cast<int32>(static_cast<int64>(LOD.IndexBuffer[First + 1]) - static_cast<int64>(Section.BaseVertexIndex));
					const int32 C = static_cast<int32>(static_cast<int64>(LOD.IndexBuffer[First + 2]) - static_cast<int64>(Section.BaseVertexIndex));
					if (!Section.SoftVertices.IsValidIndex(A) || !Section.SoftVertices.IsValidIndex(B) || !Section.SoftVertices.IsValidIndex(C))
					{
						continue;
					}
					const FSoftSkinVertex& VA = Section.SoftVertices[A];
					const FSoftSkinVertex& VB = Section.SoftVertices[B];
					const FSoftSkinVertex& VC = Section.SoftVertices[C];
					const FVector PA(VA.Position), PB(VB.Position), PC(VC.Position);
					const FVector2D UA(VA.UVs[0]), UB(VB.UVs[0]), UC(VC.UVs[0]);
					const double Area = 0.5 * FVector::CrossProduct(PB - PA, PC - PA).Size();
					const double AreaUV = 0.5 * FMath::Abs((UB.X - UA.X) * (UC.Y - UA.Y) - (UB.Y - UA.Y) * (UC.X - UA.X));
					SumU += UA.X + UB.X + UC.X;
					CountU += 3;
					if (Area > 1.0e-9 && AreaUV > 1.0e-12)
					{
						Scales.Emplace(FMath::Sqrt(Area / AreaUV), Area);
					}
				}
			}
			if (Scales.Num() < 16 || CountU == 0)
			{
				continue;
			}
			// Area-weighted median: half of the surface is finer, half coarser.
			Scales.Sort([](const TPair<double, double>& L, const TPair<double, double>& R) { return L.Key < R.Key; });
			double Total = 0.0;
			for (const TPair<double, double>& Item : Scales)
			{
				Total += Item.Value;
			}
			double Running = 0.0;
			for (const TPair<double, double>& Item : Scales)
			{
				Running += Item.Value;
				if (Running >= Total * 0.5)
				{
					Result.CmPerUV = Item.Key;
					break;
				}
			}
			Result.bBody = SumU / static_cast<double>(CountU) > 1.0;
			Result.Triangles = Scales.Num();
			Result.bMeasured = Result.CmPerUV > 1.0;
		}
		return Result;
	}

	/** Makes (or refreshes) the sweat versions for one skin slot. */
	void AuthorSlot(FContext& Context, UMaterialInterface* Material, const USkeletalMesh* Mesh, int32 MaterialIndex)
	{
		TArray<UMaterialInstanceConstant*> Instances;
		UMaterial* Base = nullptr;
		if (!Material || !GetChain(Material, Instances, Base) || !Base || !IsSkinMaster(Base))
		{
			return;  // not skin: eyes, teeth, lashes, clothes
		}
		++Context.SkinSlots;
		if (Instances.IsEmpty())
		{
			Context.Report.Problem(FString::Printf(TEXT("%s slot %d uses the skin master %s directly. Give the slot a material instance of it, then add sweat again."), *GetNameSafe(Mesh), MaterialIndex, *Base->GetName()));
			return;
		}
		UMaterialInstanceConstant* DryLeaf = Instances[0];
		const FString DryKey = DryLeaf->GetPathName();
		if (Context.DoneLeaves.Contains(DryKey))
		{
			return;
		}
		Context.DoneLeaves.Add(DryKey);

		const FSlotMeasure Measure = MeasureSlot(Mesh, MaterialIndex);
		const bool bBody = Measure.bMeasured ? Measure.bBody : (Mesh && Mesh->GetName().Contains(TEXT("Body")));
		UTexture2D* FaceMask = EnsureMask(Context, false);
		UTexture2D* BodyMask = EnsureMask(Context, true);
		if (!FaceMask || !BodyMask)
		{
			return;
		}
		UMaterial* Master = EnsureMaster(Context, Base, FaceMask);
		if (!Master)
		{
			return;
		}

		// Copy the chain from the instance nearest the master down to the character's own material.
		const FString PackagePath = Context.Folder / TEXT("Materials");
		UMaterialInterface* Parent = Master;
		UMaterialInstanceConstant* SweatLeaf = nullptr;
		bool bLeafIsNew = false;
		for (int32 Index = Instances.Num() - 1; Index >= 0; --Index)
		{
			UMaterialInstanceConstant* Source = Instances[Index];
			const FString Name = MetaHumanSweat::SweatAssetName(TEXT("MI_Sweat"), Source->GetOutermost()->GetName(), Source->GetName());
			UMaterialInstanceConstant* Copy = LoadIfExists<UMaterialInstanceConstant>(PackagePath / Name);
			const bool bNew = Copy == nullptr;
			if (bNew)
			{
				Copy = Cast<UMaterialInstanceConstant>(FAssetToolsModule::GetModule().Get().DuplicateAsset(Name, PackagePath, Source));
				if (!Copy)
				{
					Context.Report.Problem(FString::Printf(TEXT("Could not copy %s to %s/%s."), *Source->GetPathName(), *PackagePath, *Name));
					return;
				}
				++Context.Report.Created;
			}
			if (Copy->Parent != Parent)
			{
				UMaterialEditingLibrary::SetMaterialInstanceParent(Copy, Parent);
				UMaterialEditingLibrary::UpdateMaterialInstance(Copy);
				Context.Touch(Copy);
			}
			Parent = Copy;
			SweatLeaf = Copy;
			bLeafIsNew = bNew;
		}
		if (!SweatLeaf)
		{
			return;
		}

		// The last copy carries what differs per mesh: which mask, and how many bead cells fit across one UV unit.
		UTexture* CurrentMask = UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(SweatLeaf, MetaHumanSweat::Param::Mask);
		UTexture2D* WantedMask = bBody ? BodyMask : FaceMask;
		const bool bKeepsOtherPart = !bLeafIsNew && (CurrentMask == FaceMask || CurrentMask == BodyMask) && CurrentMask != WantedMask
			&& SweatLeaf->TextureParameterValues.ContainsByPredicate([](const FTextureParameterValue& Value) { return Value.ParameterInfo.Name == MetaHumanSweat::Param::Mask; });
		if (bKeepsOtherPart)
		{
			// The same material instance is used on the face mesh and on the body mesh (MetaHuman low LODs do this). One material has one mask: the first use decides.
			Context.Report.Say(FString::Printf(TEXT("%s is also used on %s slot %d, which looks like the %s; it keeps the mask of its first use."), *DryLeaf->GetName(), *GetNameSafe(Mesh), MaterialIndex, bBody ? TEXT("body") : TEXT("face")));
			return;
		}
		const double CellSize = FMath::Clamp(static_cast<double>(UMetaHumanSweatSettings::Get().BeadCellSizeCm), 0.2, 2.0);
		const double CmPerUV = Measure.bMeasured ? Measure.CmPerUV : (bBody ? DefaultBodyCmPerUV : DefaultFaceCmPerUV);
		const float Cells = static_cast<float>(FMath::Max(8.0, CmPerUV / CellSize));
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(SweatLeaf, MetaHumanSweat::Param::Mask, WantedMask);
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(SweatLeaf, MetaHumanSweat::Param::Cells, Cells);
		UMaterialEditingLibrary::UpdateMaterialInstance(SweatLeaf);
		Context.Touch(SweatLeaf);
		Context.Report.Say(FString::Printf(TEXT("%s %s -> %s (%s mask, %.1f cm per UV %s, %.1f cells per UV, chain of %d)"), bLeafIsNew ? TEXT("Created") : TEXT("Refreshed"), *DryLeaf->GetPathName(), *SweatLeaf->GetName(),
			bBody ? TEXT("body") : TEXT("face"), CmPerUV, Measure.bMeasured ? TEXT("measured") : TEXT("assumed: the mesh has no geometry to measure"), Cells, Instances.Num()));
	}

	/** Saves what the call touched. A copied instance can hold texture overrides its new parent does not use yet; they are kept as they are. */
	void Save(FContext& Context)
	{
		if (Context.ToSave.IsEmpty())
		{
			return;
		}
		IConsoleVariable* Cleanup = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Material.CleanupTextureOverrides"));
		const int32 Previous = Cleanup ? Cleanup->GetInt() : 0;
		if (Cleanup)
		{
			Cleanup->Set(0, ECVF_SetByCode);
		}
		const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Context.ToSave, false);
		if (Cleanup)
		{
			Cleanup->Set(Previous, ECVF_SetByCode);
		}
		if (bSaved)
		{
			Context.Report.Say(FString::Printf(TEXT("Saved %d package(s) under %s."), Context.ToSave.Num(), *Context.Folder));
		}
		else
		{
			Context.Report.Problem(FString::Printf(TEXT("Saving under %s failed for at least one package (is a file read-only or open elsewhere?)."), *Context.Folder));
		}
	}

	/** Nothing references the sweat materials (the component finds them by name), so a packaged game only has them if their folder is always cooked. */
	void EnsureCooked(FContext& Context)
	{
		UProjectPackagingSettings* Packaging = GetMutableDefault<UProjectPackagingSettings>();
		if (!Packaging)
		{
			return;
		}
		for (const FDirectoryPath& Directory : Packaging->DirectoriesToAlwaysCook)
		{
			if (Directory.Path.Equals(Context.Folder, ESearchCase::IgnoreCase))
			{
				return;
			}
		}
		FDirectoryPath Directory;
		Directory.Path = Context.Folder;
		Packaging->DirectoriesToAlwaysCook.Add(Directory);
		Packaging->TryUpdateDefaultConfigFile();
		Context.Report.Say(FString::Printf(TEXT("Added %s to Project Settings > Packaging > Additional Asset Directories to Cook, so packaged builds include the sweat materials."), *Context.Folder));
	}

	FString Finish(FContext& Context, const TCHAR* What)
	{
		Save(Context);
		if (Context.Report.Created > 0)
		{
			EnsureCooked(Context);
		}
		Context.Report.Lines.Insert(FString::Printf(TEXT("MetaHuman Sweat, %s: %d skin slot(s) found, %d asset(s) created, %d problem(s)."), What, Context.SkinSlots, Context.Report.Created, Context.Report.Problems), 0);
		if (Context.SkinSlots == 0 && Context.Report.Problems == 0)
		{
			Context.Report.Lines.Add(TEXT("No skin material was found. A skin material is a material instance whose base material is named in Project Settings > Plugins > MetaHuman Sweat > Skin Material Names (default M_skin_unified_baked)."));
		}
		return Context.Report.Text();
	}
}

FString UMetaHumanSweatEditorLibrary::AddSweatToMeshes(const TArray<USkeletalMesh*>& Meshes)
{
	using namespace MetaHumanSweatAuthoring;
	FContext Context;
	Context.Folder = UMetaHumanSweatSettings::Get().GetFolder();
	for (const USkeletalMesh* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		const TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			AuthorSlot(Context, Materials[Index].MaterialInterface, Mesh, Index);
		}
	}
	return Finish(Context, TEXT("Add Sweat to Meshes"));
}

FString UMetaHumanSweatEditorLibrary::AddSweatToActors(const TArray<AActor*>& Actors, bool bAddComponent)
{
	using namespace MetaHumanSweatAuthoring;
	FContext Context;
	Context.Folder = UMetaHumanSweatSettings::Get().GetFolder();
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		const int32 Before = Context.SkinSlots;
		TInlineComponentArray<USkeletalMeshComponent*> Components(Actor);
		for (USkeletalMeshComponent* Component : Components)
		{
			const USkeletalMesh* Mesh = Component ? Component->GetSkeletalMeshAsset() : nullptr;
			if (!Mesh)
			{
				continue;
			}
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
			{
				// A Blueprint's construction script may already have put a dynamic instance in the slot: the asset it was made from is what counts.
				UMaterialInterface* Material = Component->GetMaterial(Slot);
				for (int32 Depth = 0; Depth < 16; ++Depth)
				{
					UMaterialInstanceDynamic* Dynamic = Cast<UMaterialInstanceDynamic>(Material);
					if (!Dynamic)
					{
						break;
					}
					Material = Dynamic->Parent;
				}
				AuthorSlot(Context, Material, Mesh, Slot);
			}
		}
		if (bAddComponent && Context.SkinSlots > Before && !Actor->FindComponentByClass<UMetaHumanSweatComponent>())
		{
			Actor->Modify();
			UMetaHumanSweatComponent* Component = NewObject<UMetaHumanSweatComponent>(Actor, TEXT("MetaHumanSweat"), RF_Transactional);
			Actor->AddInstanceComponent(Component);
			Component->RegisterComponent();
			Context.Report.Say(FString::Printf(TEXT("Added a MetaHuman Sweat component to %s (on this actor only: add it in the Blueprint to have it on every copy). Save the level to keep it."), *Actor->GetActorNameOrLabel()));
		}
	}
	return Finish(Context, TEXT("Add Sweat to Actors"));
}

FString UMetaHumanSweatEditorLibrary::UpdateSweatMaterials()
{
	using namespace MetaHumanSweatAuthoring;
	FContext Context;
	Context.Folder = UMetaHumanSweatSettings::Get().GetFolder();
	TArray<FAssetData> Assets;
	IAssetRegistry::GetChecked().GetAssetsByPath(FName(*(Context.Folder / TEXT("Materials"))), Assets, false);
	int32 Rebuilt = 0;
	for (const FAssetData& Asset : Assets)
	{
		UMaterial* Material = Cast<UMaterial>(Asset.GetAsset());
		if (!Material || !HasBlock(Material))
		{
			continue;
		}
		// Keep the mask the block already shows by default.
		UTexture2D* DefaultMask = nullptr;
		for (const TObjectPtr<UMaterialExpression>& Expression : Material->GetExpressions())
		{
			if (const UMaterialExpressionTextureSampleParameter2D* Sample = Cast<UMaterialExpressionTextureSampleParameter2D>(Expression))
			{
				if (Sample->ParameterName == MetaHumanSweat::Param::Mask)
				{
					DefaultMask = Cast<UTexture2D>(Sample->Texture);
				}
			}
		}
		if (!DefaultMask)
		{
			DefaultMask = EnsureMask(Context, false);
		}
		if (!DefaultMask || !RemoveBlock(Material, Context.Report) || !BuildBlock(Material, DefaultMask, Context.Report) || !Recompile(Material, Context.Report))
		{
			continue;
		}
		Context.Touch(Material);
		++Rebuilt;
		Context.Report.Say(FString::Printf(TEXT("Rebuilt the sweat block of %s"), *Material->GetPathName()));
	}
	Save(Context);
	Context.Report.Lines.Insert(FString::Printf(TEXT("MetaHuman Sweat, Update Sweat Materials: %d master material(s) rebuilt, %d problem(s)."), Rebuilt, Context.Report.Problems), 0);
	if (Rebuilt == 0 && Context.Report.Problems == 0)
	{
		Context.Report.Lines.Add(FString::Printf(TEXT("No sweat master material was found under %s/Materials. Add sweat to a character first."), *Context.Folder));
	}
	return Context.Report.Text();
}
