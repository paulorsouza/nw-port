#include "NWEvents.h"

FNWEvents& FNWEvents::Get()
{
	static FNWEvents Instance;
	return Instance;
}
