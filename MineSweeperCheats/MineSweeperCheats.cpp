#include <Windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <stdbool.h>

#define ERR_EXIT(CAUSE) perror(CAUSE);\
exit(1);

struct gameHandle_t
{
	int pid;
	HANDLE hProc;
	bool hasFound;

} typedef gameHandle_t;

gameHandle_t g_gameHandler;

void gameFinder() {
	if (g_gameHandler.hasFound) {
		return;
	}

	HANDLE hProcessSnap;
	HANDLE hProcess;
	PROCESSENTRY32 pe32;
	DWORD dwPriorityClass;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

	if (hProcessSnap == INVALID_HANDLE_VALUE || hProcessSnap == NULL)
	{
		ERR_EXIT("CreateToolhelp32Snapshot");
	}

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32))
	{
		ERR_EXIT("Process32First");
	}

	do
	{
		WCHAR* procName_lower;

		WCHAR* procName = pe32.szExeFile;
		int pid = pe32.th32ProcessID;

		int lowcase_length = wcslen(procName) * 2;
		procName_lower = (WCHAR*)malloc(lowcase_length + 1);
		procName_lower[0] = '\x00';

		wcscpy_s(procName_lower, lowcase_length + 1, procName);
		_wcslwr_s(procName_lower, lowcase_length + 1);

		HANDLE tempHandle;

		if (wcscmp(L"winmine.exe", procName_lower) == 0) {
			g_gameHandler.pid = pe32.th32ProcessID;

			if ((tempHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, g_gameHandler.pid)) == NULL) {
				ERR_EXIT("OpenProcess");
			}

			g_gameHandler.hProc = tempHandle;
			g_gameHandler.hasFound = true;
		}

		free(procName_lower);
	} while (Process32Next(hProcessSnap, &pe32) && !g_gameHandler.hasFound);

	CloseHandle(hProcessSnap);
}

void injectDll(HANDLE hProc, char* dllPath) {

	LPVOID remoteAddress;
	int dllPathLen = strlen(dllPath) + 1;

	if ((remoteAddress = VirtualAllocEx(hProc,
		NULL,
		dllPathLen,
		MEM_COMMIT,
		PAGE_READWRITE)) == NULL) {

		ERR_EXIT("VirtualAllocEx");
	}

	if (!WriteProcessMemory(hProc,
		remoteAddress,
		dllPath,
		dllPathLen,
		NULL)) {
		ERR_EXIT("WriteProcessMemory");
	}

	HMODULE hModKernel32;

	if ((hModKernel32 = GetModuleHandleA("Kernel32.dll")) == NULL) {
		ERR_EXIT("LoadLibraryA");
	}

	HANDLE hLoadLibraryA;

	if ((hLoadLibraryA = GetProcAddress(hModKernel32, "LoadLibraryA")) == NULL) {
		ERR_EXIT("GetProcAddress");
	}

	if (CreateRemoteThread(hProc,
		NULL,
		NULL,
		(LPTHREAD_START_ROUTINE)hLoadLibraryA,
		remoteAddress,
		NULL,
		NULL
	) == NULL) {
		ERR_EXIT("CreateRemoteThread");
	}
}

char* getCurrDllPath() {
	int bufSize = 256;
	char* fullPath = (char*)malloc(bufSize + 1);

	if (fullPath == NULL) {
		exit(1);
	}

	fullPath[0] = '\00';
	GetCurrentDirectoryA(bufSize, fullPath);

	strcat_s(fullPath, bufSize, (const char*)"\\");
	strcat_s(fullPath, bufSize, (const char*)"MineSweeperCore.dll");

	return fullPath;
}

bool hasCheatsInjected() {
	HANDLE hMutex;

	if ((hMutex = OpenMutexA(READ_CONTROL, FALSE, "WinMineCheats")) == NULL) {
		return false;
	}
	else {
		CloseHandle(hMutex);
		return true;
	}
}

int main()
{
	g_gameHandler = { 0 };

	puts("MineSweeper Cheats V1.0");
	puts("-----------------------\n");
	puts("");

	do {
		gameFinder();
		Sleep(500);
	} while (!g_gameHandler.hasFound);

	puts("Game process found!\n");

	char* fullDllPath = getCurrDllPath();
	printf("DLL Path --> %s\n", fullDllPath);
	puts("Tring to inject to process");

	do {
		injectDll(g_gameHandler.hProc, fullDllPath);
		Sleep(1000);
	} while (!hasCheatsInjected());

	puts("Sucsess");

}
