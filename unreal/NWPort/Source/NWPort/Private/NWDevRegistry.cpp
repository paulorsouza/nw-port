#include "NWDevRegistry.h"

TArray<FNWDevEntry>& FNWDevRegistry::All()
{
	static TArray<FNWDevEntry> Entries;
	return Entries;
}

bool FNWDevRegistry::Add(FNWDevEntry Entry)
{
	check(!Find(Entry.Id));
	All().Add(MoveTemp(Entry));
	return true;
}

const FNWDevEntry* FNWDevRegistry::Find(FName Id)
{
	return All().FindByPredicate([Id](const FNWDevEntry& E) { return E.Id == Id; });
}
