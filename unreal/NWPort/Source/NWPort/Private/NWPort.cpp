#include "Misc/NetworkVersion.h"
#include "Modules/ModuleManager.h"

/**
 * Game network version. Unreal's includes the changelist of the engine build: the server (compiled from
 * source) and the clients (installed engine, Linux or Windows) would give different numbers and the connection would
 * be rejected as "incompatible version". Bump NetProtocol when changing anything replicated (properties, RPCs).
 */
class FNWPortModule : public FDefaultGameModuleImpl
{
	static constexpr uint32 NetProtocol = 3;   // 2: inventory (F3); 3: gathering, progression and trade skills, unstuck

	virtual void StartupModule() override
	{
		FNetworkVersion::GetLocalNetworkVersionOverride.BindLambda([]()
		{
			return FCrc::StrCrc32(*FString::Printf(TEXT("NWPort/net/%u"), NetProtocol));
		});
	}

	virtual void ShutdownModule() override
	{
		FNetworkVersion::GetLocalNetworkVersionOverride.Unbind();
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FNWPortModule, NWPort, "NWPort");
