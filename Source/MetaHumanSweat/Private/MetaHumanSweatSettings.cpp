#include "MetaHumanSweatSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MetaHumanSweatSettings)

UMetaHumanSweatSettings::UMetaHumanSweatSettings()
{
	SkinMaterialNames.Add(TEXT("M_skin_unified_baked"));
}

FString UMetaHumanSweatSettings::GetFolder() const
{
	FString Folder = MaterialFolder.TrimStartAndEnd();
	while (Folder.EndsWith(TEXT("/")))
	{
		Folder.LeftChopInline(1);
	}
	return Folder.IsEmpty() ? FString(TEXT("/Game/MetaHumanSweat")) : Folder;
}
