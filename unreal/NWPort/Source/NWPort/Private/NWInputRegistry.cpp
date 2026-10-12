#include "NWInputRegistry.h"

TArray<FNWInputBinding>& FNWInputRegistry::All()
{
	static TArray<FNWInputBinding> Bindings;
	return Bindings;
}

bool FNWInputRegistry::Add(FNWInputBinding Binding)
{
	check(!All().ContainsByPredicate([&](const FNWInputBinding& B) { return B.Id == Binding.Id; }));
	All().Add(MoveTemp(Binding));
	return true;
}
