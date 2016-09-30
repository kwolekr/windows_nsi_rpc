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
	DWORD *pdwModuleId,
	DWORD dwSomeNumber,
	void *arg5,
	void *arg6,
	PVOID pBufferOut,
	DWORD dwBufferOutLenLen,
	void *arg9,
	void *arg10,
	void *arg11,
	void *arg12);


DWORD NPI_MS_UDP_MODULEID[] = {
	0x18, 0x01, 0x0EB004A02, 0x11D49B1A, 0x50002391, 0x0BC597704};
DWORD NPI_MS_TCP_MODULEID[] = {
	0x18, 0x01, 0x0EB004A03, 0x11D49B1A, 0x50002391, 0x0BC597704};


int main()
{
	NSI_CONNECT_TO_SERVER NsiConnectToServer;
	NSI_DISCONNECT_FROM_SERVER NsiDisconnectFromServer;
	NSI_RPC_GET_ALL_PARAMETERS NsiRpcGetAllParameters;
	HMODULE hWinNsi;
	RPC_STATUS status;
	RPC_BINDING_HANDLE hNsiServer;

	BOOL is_udp = FALSE;
	DWORD *module_id;
	DWORD mysterious_value;
	DWORD store;
	DWORD buffer_out = 0;

	hWinNsi = LoadLibrary("winnsi.dll");
	if (!hWinNsi) {
		printf("Failed to load winnsi.dll\n");
		return 1;
	}

	NsiConnectToServer = (NSI_CONNECT_TO_SERVER)GetProcAddress(
		hWinNsi, "NsiConnectToServer");
	NsiDisconnectFromServer = (NSI_DISCONNECT_FROM_SERVER)GetProcAddress(
		hWinNsi, "NsiDisconnectFromServer");
	NsiRpcGetAllParameters = (NSI_RPC_GET_ALL_PARAMETERS)GetProcAddress(
		hWinNsi, "NsiRpcGetAllParameters");
	if (!NsiConnectToServer || !NsiDisconnectFromServer || !NsiRpcGetAllParameters) {
		printf("Failed to get one or more library exports\n");
		return 1;
	}

	hNsiServer = NsiConnectToServer(NULL);
	if (!hNsiServer) {
		printf("NsiConnectToServer failed\n");
		return 1;
	}

	store = 1;  // 1 = active, 0 = persistent
	module_id = is_udp ? NPI_MS_UDP_MODULEID : NPI_MS_TCP_MODULEID;
	mysterious_value = is_udp ? 4 : 20;

	status = NsiRpcGetAllParameters(hNsiServer, store, module_id,
		mysterious_value, NULL, NULL, &buffer_out, sizeof(buffer_out),
		NULL, NULL, NULL, NULL);

	if (status == ERROR_NOT_FOUND) {
		printf("No info found for this store\n");
	} else if (status == RPC_S_OK) {
		unsigned int start_port =
			((buffer_out << 8) & 0xFF00) | ((buffer_out >> 8) & 0x00FF);
		unsigned int num_ports = buffer_out >> 16;

		printf("Ephemeral port range for %s protocol:\n"
			"Start port: %d\n"
			"Number of ports: %d\n",
			is_udp ? "udp" : "tcp", start_port, num_ports);
	} else {
		printf("NsiRpcGetAllParameters failed, status=%lu\n", status);
	}

	status = NsiDisconnectFromServer(hNsiServer);
	if (status != RPC_S_OK) {
		printf("NsiDisconnectFromServer failed, status=%lu\n", status);
	}

	FreeLibrary(hWinNsi);

	return 0;
}
