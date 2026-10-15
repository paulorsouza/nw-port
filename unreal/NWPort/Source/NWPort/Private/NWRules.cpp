#include "NWRules.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogNWRules, Log, All);

double FNWRules::Num(const TCHAR* Id, double Default)
{
	static TMap<FString, double> Values;
	static TSet<FString> Warned;
	static bool bLoaded = false;
	if (!bLoaded)
	{
		bLoaded = true;
		FString Raw;
		TSharedPtr<FJsonObject> Root;
		const FString File = FPaths::ProjectDir() / TEXT("Data/regras.json");
		if (FFileHelper::LoadFileToString(Raw, *File) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Raw), Root) && Root.IsValid())
		{
			for (const auto& Pair : Root->Values)
			{
				double V = 0.0;
				if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(V))
				{
					Values.Add(FString(Pair.Key), V);
				}
			}
		}
		else
		{
			UE_LOG(LogNWRules, Log, TEXT("%s missing: code defaults"), *File);
		}
	}
	if (const double* V = Values.Find(Id))
	{
		return *V;
	}
	if (!Warned.Contains(Id))
	{
		Warned.Add(Id);
		UE_LOG(LogNWRules, Verbose, TEXT("rule %s not in Data/regras.json; using %g"), Id, Default);
	}
	return Default;
}
