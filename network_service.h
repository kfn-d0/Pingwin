#ifndef NETWORK_SERVICE_H
#define NETWORK_SERVICE_H

#include "app_context.h"

void NetworkService_Start();
void NetworkService_Stop();
void NetworkService_ResolveHost(std::string host);
void NetworkService_RunTraceroute(std::string host);
void CollectNetworkData();
void NetworkService_ScanRemotePorts(std::string host);

#endif
