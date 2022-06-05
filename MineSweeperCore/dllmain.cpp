#include <Windows.h>
#include "pch.h"
#include <iostream>
#include <dbghelp.h>
#include <list>

#define VERBOSE

#define CLEAR() system("cls")

typedef int(*__stdcall flag_tile_t)(int x_cord, int y_cord, char tile_value);
typedef int(*__stdcall click_tile_t)(int x, int y);
typedef int(*__stdcall do_clearout_t)(int x, int y);

struct coord_t
{
	int x;
	int y;
} typedef coord_t;

HANDLE		cheatThread = NULL;

flag_tile_t flag_tile_inst;
click_tile_t click_tile_inst;
do_clearout_t do_clearout_inst;

char* g_mineArray;
int* g_minesLeft = NULL;
coord_t g_size = { 0 };

void SpwanConsole() {
	AllocConsole();
	FILE* fp = new FILE;
	freopen_s(&fp, "CONOUT$", "w", stdout);
}

struct ImageSectionInfo
{
	char SectionName[IMAGE_SIZEOF_SHORT_NAME];//the macro is defined WinNT.h
	char* SectionAddress;
	int SectionSize;

	ImageSectionInfo(const char* name)
	{
		strcpy_s(SectionName, IMAGE_SIZEOF_SHORT_NAME, name);
	}
};

char* GetSegmentOfModule(HMODULE hModule, const char* segmentName) {
	char* res = NULL;
	char buf[10] = { 0 };

	//store the base address the loaded Module
	char* dllImageBase = (char*)hModule; //suppose hModule is the handle to the loaded Module (.exe or .dll)

	//get the address of NT Header
	IMAGE_NT_HEADERS* pNtHdr = ImageNtHeader(hModule);

	//after Nt headers comes the table of section, so get the addess of section table
	IMAGE_SECTION_HEADER* pSectionHdr = (IMAGE_SECTION_HEADER*)(pNtHdr + 1);

	ImageSectionInfo* pSectionInfo = NULL;

	//printf("[hModule]: %p\n", hModule);

	//iterate through the list of all sections, and check the section name in the if conditon. etc
	for (int i = 0; i < pNtHdr->FileHeader.NumberOfSections; i++)
	{
		char* name = (char*)pSectionHdr->Name;
		memcpy_s(buf, 10, name, 8);

		//printf("[SECTION]: %s\n", buf);

		if (memcmp(name, segmentName, strlen(segmentName)) == 0)
		{
			pSectionInfo = new ImageSectionInfo(segmentName);

			res = dllImageBase + pSectionHdr->VirtualAddress;
		}
		pSectionHdr++;
	}

	return res;
}

/// <summary>
/// Returns Coordinates From Index, starts from 1
/// </summary>
/// <param name="index">index accessed in array</param>
/// <returns>Coordinates</returns>
coord_t getCoordinatesFromIndex(int index) {
	coord_t res = { 0 };

	res.x = index % 32;
	res.y = index / 32;

	return res;
}

int getIndexFromCoordinates(coord_t* coord) {
	int index;

	index = coord->y * 32;
	index += coord->x;

	return index;
}

void initFunctions(HMODULE hModule) {
	char* selfTextSegment = GetSegmentOfModule(hModule, ".text");
	//printf(".TEXT: %p\n", selfTextSegment);
	flag_tile_inst = (flag_tile_t)(0x1002EAB);
	//printf(".flag_tile_inst: %p\n", flag_tile_inst);

	click_tile_inst = (click_tile_t)(0x100316B);
	do_clearout_inst = (do_clearout_t)(0x1003084);
}

void initGlobals(HMODULE hModule) {
	char* selfDataSegment = GetSegmentOfModule(hModule, ".data");

	if (selfDataSegment == NULL) {
		perror("minesLeft");
		exit(1);
	}

	g_mineArray = selfDataSegment + 0x340;
	g_minesLeft = (int*)(selfDataSegment + 404);
}

void updateSize(HMODULE hModule) {
	char* selfDataSegment = GetSegmentOfModule(hModule, ".data");

	g_size.x = *(((int*)(selfDataSegment + 0x334)));
	g_size.y = *(((int*)(selfDataSegment + 0x338)));
}

void emptyClick(coord_t *coord) {
	int x, y;
	x = coord->x;
	y = coord->y;

	(*click_tile_inst)(x, y);

	__asm {
		sub esp, 8
	}

	(*do_clearout_inst)(x, y);

	__asm {
		sub esp, 8
	}

}

void markAllMinesEx(HMODULE hModule) {
	const int ARRAY_MAX = 0x360;

	const char TILE_MINE = 0x8F;
	const char TILE_MINE_FLAGGED = 0x8E;

	coord_t indexCoord = { 0 };
	int minesSpotted = 0;

	puts("Marking");

	int index;
	char* ptr;
	int initialMines = *g_minesLeft;

	ptr = g_mineArray;
	for (int counter = 0; counter < ARRAY_MAX; ++counter)
	{
		indexCoord = getCoordinatesFromIndex(counter);

		if (*ptr == TILE_MINE) {

#ifdef VERBOSE

			printf("Mine %d - ", ++minesSpotted);
			printf("(%d, %d)\n", indexCoord.x, indexCoord.y);
#endif // VERBOSE


			(*flag_tile_inst)(indexCoord.x, indexCoord.y, TILE_MINE_FLAGGED);

			__asm {
				sub esp, 0xC
			}

			Sleep(100);
		}
		else {
			if ((indexCoord.x >= 1 && indexCoord.x <= g_size.x) && (indexCoord.y >= 1 && indexCoord.y <= g_size.y)) {
				emptyClick(&indexCoord);
			}
		}

		if (minesSpotted >= initialMines) {
			break;
		}

		++ptr;	
	}

	*g_minesLeft = 0;

}

DWORD WINAPI HookGame(HMODULE hModule) {
#ifdef VERBOSE
	SpwanConsole();
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, 2);
#endif

	bool threadRun = true;

	HMODULE hSelf = GetModuleHandleA("WINMINE.exe");

	if (hSelf == NULL) {
		exit(1);
	}

	CreateMutexA(NULL, FALSE, "WinMineCheats");

	int* timer = (int*)((char*)hSelf + 0x579C);

	initFunctions(hSelf);
	initGlobals(hSelf);


	while (threadRun) {
		CLEAR();
		updateSize(hSelf);

#ifdef VERBOSE
		printf("Mines left: %d\n", *g_minesLeft);
		std::cout << "Timer: " << (*timer) << std::endl;
		std::cout << "Size: " << g_size.x << 'x' << g_size.y << std::endl;
#endif // VERBOSE


		bool hasCheated = false;

		if ((*timer) > 0) {
			markAllMinesEx(hSelf);

			hasCheated = true;
		}

		Sleep(250);
	}

	return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{

	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		// Initialize once for each new process.
		// Return FALSE to fail DLL load.
		break;

	case DLL_THREAD_ATTACH:

		if (cheatThread == NULL)
		{
			cheatThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)HookGame, hModule, 0, nullptr);
		}

		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;

	case DLL_PROCESS_DETACH:
		// Perform any necessary cleanup.
		break;
	}

	return TRUE;
}

