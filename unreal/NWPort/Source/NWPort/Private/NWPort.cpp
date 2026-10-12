#include "Misc/NetworkVersion.h"
#include "Modules/ModuleManager.h"

/**
 * Versão de rede do jogo. A da Unreal inclui o changelist da build da engine: o servidor (compilado do
 * código-fonte) e os clientes (engine instalada, Linux ou Windows) dariam números diferentes e a conexão seria
 * recusada como "versão incompatível". Aumente NetProtocol quando mudar algo replicado (propriedades, RPCs).
 */
class FNWPortModule : public FDefaultGameModuleImpl
{
	static constexpr uint32 NetProtocol = 2;   // 2: inventário (F3)

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
