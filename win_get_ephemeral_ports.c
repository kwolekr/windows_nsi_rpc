/*-
 * Copyright (c) 2016 Ryan Kwolek
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright notice, this list of
 *     conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice, this list
 *     of conditions and the following disclaimer in the documentation and/or other materials
 *     provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * win_get_ephemeral_ports.c -
 *    An example of how to programatically get the ephemeral port range on
 *    Windows Vista and up.  Got most of this from reversing netiohlp.dll
 *    and winnsi.dll.
 *    This also serves as a basic example of how to interact with the entirely
 *    undocumented Windows NSI, the Network Store Information manager, using its
 *    external RPC interface to the "nsi" Service.
 */

#include <windows.h>
#include <stdio.h>

typedef RPC_BINDING_HANDLE (__stdcall *NSI_CONNECT_TO_SERVER)(
	void *arg1);

typedef RPC_STATUS (__stdcall *NSI_DISCONNECT_FROM_SERVER)(
	RPC_BINDING_HANDLE hServerConn);

typedef RPC_STATUS (__stdcall *NSI_RPC_GET_ALL_PARAMETERS)(
	RPC_BINDING_HANDLE hServerConn,
	DWORD dwStore,
	LPDWORD lpModuleId,
	DWORD dwSomeNumber,
	void *arg5,
	void *arg6,
	LPVOID lpOutBuffer,
	DWORD nOutBufferSize,
	void *arg9,
	void *arg10,
	void *arg11,
	void *arg12);


DWORD NPI_MS_UDP_MODULEID[] = {
	0x18, 0x01, 0x0EB004A02, 0x11D49B1A, 0x50002391, 0x0BC597704};
DWORD NPI_MS_TCP_MODULEID[] = {
	0x18, 0x01, 0x0EB004A03, 0x11D49B1A, 0x50002391, 0x0BC597704};

#define NPI_STORE_PERSISTENT 0
#define NPI_STORE_ACTIVE     1


BOOL GetDynamicPortRangeFromNsi(
	int protocol,
	unsigned int *start_port,
	unsigned int *number_of_ports)
{
	NSI_CONNECT_TO_SERVER NsiConnectToServer;
	NSI_DISCONNECT_FROM_SERVER NsiDisconnectFromServer;
	NSI_RPC_GET_ALL_PARAMETERS NsiRpcGetAllParameters;
	HMODULE hWinNsi;
	BOOLEAN success;
	RPC_STATUS rpcstatus;
	RPC_BINDING_HANDLE hNsiServer;

	DWORD *module_id;
	DWORD mysterious_value;
	DWORD buffer = 0;

	switch (protocol) {
		case IPPROTO_TCP:
			module_id = NPI_MS_TCP_MODULEID;
			mysterious_value = 20;
			break;
		case IPPROTO_UDP:
			module_id = NPI_MS_UDP_MODULEID;
			mysterious_value = 4;
			break;
		default:
			printf("Unhandled protocol %d\n", protocol);
			return FALSE;
	}

	hWinNsi = LoadLibrary("winnsi.dll");
	if (!hWinNsi) {
		printf("Failed to load winnsi.dll\n");
		return FALSE;
	}

	NsiConnectToServer = (NSI_CONNECT_TO_SERVER)GetProcAddress(
		hWinNsi, "NsiConnectToServer");
	NsiDisconnectFromServer = (NSI_DISCONNECT_FROM_SERVER)GetProcAddress(
		hWinNsi, "NsiDisconnectFromServer");
	NsiRpcGetAllParameters = (NSI_RPC_GET_ALL_PARAMETERS)GetProcAddress(
		hWinNsi, "NsiRpcGetAllParameters");
	if (!NsiConnectToServer || !NsiDisconnectFromServer ||
			!NsiRpcGetAllParameters) {
		printf("Failed to get one or more library exports\n");
		FreeLibrary(hWinNsi);
		return FALSE;
	}

	hNsiServer = NsiConnectToServer(NULL);
	if (!hNsiServer) {
		printf("NsiConnectToServer failed\n");
		FreeLibrary(hWinNsi);
		return FALSE;
	}

	rpcstatus = NsiRpcGetAllParameters(hNsiServer, NPI_STORE_ACTIVE, module_id,
		mysterious_value, NULL, NULL, &buffer, sizeof(buffer),
		NULL, NULL, NULL, NULL);

	if (rpcstatus == RPC_S_OK) {
		*start_port = ((buffer << 8) & 0xFF00) | ((buffer >> 8) & 0x00FF);
		*number_of_ports = buffer >> 16;
		success = TRUE;
	} else {
		printf("NsiRpcGetAllParameters failed, rpcstatus=%lu\n", rpcstatus);
		success = FALSE;
	}

	rpcstatus = NsiDisconnectFromServer(hNsiServer);
	if (rpcstatus != RPC_S_OK) {
		printf("NsiDisconnectFromServer failed, rpcstatus=%lu\n", rpcstatus);
		success = FALSE;
	}

	FreeLibrary(hWinNsi);

	return success;
}


int main(int argc, char *argv[])
{
	unsigned int start_port;
	unsigned int num_ports;
	int protocol = IPPROTO_TCP;
	const char *protocol_str = "TCP";

	if (argc > 1) {
		if (!stricmp(argv[1], "tcp")) {
			protocol = IPPROTO_TCP;
			protocol_str = "TCP";
		} else if (!stricmp(argv[1], "udp")) {
			protocol = IPPROTO_UDP;
			protocol_str = "UDP";
		} else {
			printf("Unsupported protocol '%s'\n", argv[1]);
			return 1;
		}
	}

	if (!GetDynamicPortRangeFromNsi(protocol, &start_port, &num_ports)) {
		printf("Failed to get dynamic port range.\n");
		return 1;
	}

	printf("Ephemeral port range for %s protocol:\n"
		"Start port: %d\n"
		"Number of ports: %d\n",
		protocol_str, start_port, num_ports);

	return 0;
}
