#include "MetaHumanSweatEditorLibrary.h"

#include "AssetRegistry/AssetData.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Selection.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "MetaHumanSweatEditor"

namespace MetaHumanSweatMenu
{
	void Show(const FString& Report)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Report), LOCTEXT("ReportTitle", "MetaHuman Sweat"));
	}

	void AddToSelectedActors()
	{
		TArray<AActor*> Actors;
		if (GEditor)
		{
			for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
			{
				if (AActor* Actor = Cast<AActor>(*It))
				{
					Actors.Add(Actor);
				}
			}
		}
		if (Actors.IsEmpty())
		{
			Show(TEXT("Select one or more MetaHuman actors in the level first, then run this again."));
			return;
		}
		Show(UMetaHumanSweatEditorLibrary::AddSweatToActors(Actors, true));
	}

	void AddToSelectedMeshes()
	{
		TArray<FAssetData> Selected;
		FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser")).Get().GetSelectedAssets(Selected);
		TArray<USkeletalMesh*> Meshes;
		for (const FAssetData& Asset : Selected)
		{
			if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Asset.GetAsset()))
			{
				Meshes.Add(Mesh);
			}
		}
		if (Meshes.IsEmpty())
		{
			Show(TEXT("Select a MetaHuman's body and face skeletal meshes in the Content Browser first, then run this again."));
			return;
		}
		Show(UMetaHumanSweatEditorLibrary::AddSweatToMeshes(Meshes));
	}

	void Update()
	{
		Show(UMetaHumanSweatEditorLibrary::UpdateSweatMaterials());
	}

	void Register()
	{
		UToolMenu* Tools = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		if (!Tools)
		{
			return;
		}
		FToolMenuSection& Section = Tools->AddSection(TEXT("MetaHumanSweat"), LOCTEXT("Section", "MetaHuman Sweat"));
		Section.AddMenuEntry(TEXT("MetaHumanSweatActors"), LOCTEXT("Actors", "Add Sweat to Selected Actors"),
			LOCTEXT("ActorsTip", "Makes the sweat versions of the skin materials of the MetaHuman actors selected in the level, and adds a MetaHuman Sweat component to each. Run it once per character; running it again is safe."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&AddToSelectedActors)));
		Section.AddMenuEntry(TEXT("MetaHumanSweatMeshes"), LOCTEXT("Meshes", "Add Sweat to Selected Skeletal Meshes"),
			LOCTEXT("MeshesTip", "Makes the sweat versions of the skin materials of the skeletal meshes selected in the Content Browser (a MetaHuman's body and face meshes). Add the MetaHuman Sweat component to the character's Blueprint yourself."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&AddToSelectedMeshes)));
		Section.AddMenuEntry(TEXT("MetaHumanSweatUpdate"), LOCTEXT("Update", "Update Sweat Materials"),
			LOCTEXT("UpdateTip", "Rebuilds the sweat block of every sweat master material from the plugin's shader files and recompiles it. Run it after updating the plugin."),
			FSlateIcon(), FUIAction(FExecuteAction::CreateStatic(&Update)));
	}
}

class FMetaHumanSweatEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		StartupHandle = UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateStatic(&MetaHumanSweatMenu::Register));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(StartupHandle);
	}

private:
	FDelegateHandle StartupHandle;
};

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMetaHumanSweatEditorModule, MetaHumanSweatEditor)
