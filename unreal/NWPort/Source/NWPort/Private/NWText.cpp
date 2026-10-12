#include "NWText.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* SettingsSection = TEXT("NW.Settings");

	FString& CurrentLang()
	{
		static FString Lang;
		return Lang;
	}

	FString Normalize(const FString& Code)
	{
		return Code.StartsWith(TEXT("pt"), ESearchCase::IgnoreCase) ? TEXT("pt-br") : TEXT("en-us");
	}

	// chave -> (pt-br, en-us)
	const TMap<FString, TPair<FString, FString>>& Table()
	{
		static TMap<FString, TPair<FString, FString>> Map;
		static bool bLoaded = false;
		if (!bLoaded)
		{
			bLoaded = true;
			FString Raw;
			TSharedPtr<FJsonObject> Root;
			if (FFileHelper::LoadFileToString(Raw, *(FPaths::ProjectDir() / TEXT("Data/UI/nw_strings.json")))
				&& FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
			{
				for (const auto& Pair : Root->Values)
				{
					const TSharedPtr<FJsonObject> O = Pair.Value->AsObject();
					if (O.IsValid())
					{
						Map.Add(FString(*Pair.Key), {O->GetStringField(TEXT("pt-br")), O->GetStringField(TEXT("en-us"))});
					}
				}
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("NW: sem Data/UI/nw_strings.json (tools/build_strings.py)"));
			}
		}
		return Map;
	}
}

const FString& NWText::Lang()
{
	FString& Lang = CurrentLang();
	if (Lang.IsEmpty())
	{
		FString Code;
		if (FParse::Value(FCommandLine::Get(), TEXT("-nwlang="), Code)
			|| GConfig->GetString(SettingsSection, TEXT("Language"), Code, GGameUserSettingsIni))
		{
			Lang = Normalize(Code);
		}
		else
		{
			Lang = Normalize(FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName());
		}
	}
	return Lang;
}

void NWText::SetLang(const FString& Code)
{
	CurrentLang() = Normalize(Code);
	GConfig->SetString(SettingsSection, TEXT("Language"), *CurrentLang(), GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

FString NWText::Get(const FString& Key)
{
	const TPair<FString, FString>* Pair = Table().Find(Key);
	if (!Pair)
	{
		return Key;
	}
	return Lang() == TEXT("pt-br") ? Pair->Key : Pair->Value;
}

FString NWText::Format(const FString& Key, const TArray<FString>& Args)
{
	FString Text = Get(Key);
	for (int32 i = 0; i < Args.Num(); ++i)
	{
		const FString Slot = FString::Printf(TEXT("{%d}"), i);
		if (Text.Contains(Slot))
		{
			Text.ReplaceInline(*Slot, *Args[i]);
			continue;
		}
		// Textos do jogo com printf (%d, %s): o argumento i ocupa o próximo.
		int32 At = Text.Find(TEXT("%d"));
		if (At == INDEX_NONE)
		{
			At = Text.Find(TEXT("%s"));
		}
		if (At != INDEX_NONE)
		{
			Text = Text.Left(At) + Args[i] + Text.Mid(At + 2);
		}
	}
	return Text;
}

FString NWText::Key(const FString& InKey, const TArray<FString>& Args)
{
	FString Out = TEXT("#") + InKey;
	for (const FString& A : Args)
	{
		Out += TEXT("|") + A.Replace(TEXT("|"), TEXT("/"));
	}
	return Out;
}

FString NWText::Resolve(const FString& Message)
{
	if (!Message.StartsWith(TEXT("#")))
	{
		return Message;
	}
	TArray<FString> Parts;
	Message.Mid(1).ParseIntoArray(Parts, TEXT("|"), false);
	if (Parts.Num() == 0)
	{
		return Message;
	}
	const FString K = Parts[0];
	Parts.RemoveAt(0);
	return Format(K, Parts);
}

FString NWText::DataPath(const FString& Relative)
{
	const FString Localized = FPaths::ProjectDir() / FPaths::GetBaseFilename(Relative, false) + TEXT(".") + Lang() + FPaths::GetExtension(Relative, true);
	if (IFileManager::Get().FileExists(*Localized))
	{
		return Localized;
	}
	return FPaths::ProjectDir() / Relative;
}
