#include "libsys.h"

#include "common/globals_cvar.h"

typedef HMODULE (*WINAPI ILoadLibraryA)(LPCSTR lpLibFileName);
typedef HMODULE (*WINAPI ILoadLibraryExA)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE (*WINAPI ILoadLibraryW)(LPCWSTR lpLibFileName);
typedef HMODULE (*WINAPI ILoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);

ILoadLibraryA o_LoadLibraryA = nullptr;
ILoadLibraryExA o_LoadLibraryExA = nullptr;
ILoadLibraryW o_LoadLibraryW = nullptr;
ILoadLibraryExW o_LoadLibraryExW = nullptr;

//-----------------------------------------------------------------------------
// Purpose: Run detour callbacks for given HMODULE
//-----------------------------------------------------------------------------
void LibSys_RunModuleCallbacks(HMODULE hModule, int iRecurse = 0)
{
	if (!hModule || iRecurse > 1)
	{
		return;
	}

	// FIXME [Fifty]: Instead of only recursing once we should store the modules we ran callbacks for
	iRecurse++;

	// Get module base name in ASCII as noone wants to deal with unicode
	CHAR szModuleName[MAX_PATH];
	GetModuleBaseNameA(GetCurrentProcess(), hModule, szModuleName, MAX_PATH);

	// Run calllbacks for all imported modules
	CModule cModule(hModule);
	for (const std::string& svImport : cModule.GetImportedModules())
		LibSys_RunModuleCallbacks(GetModuleHandleA(svImport.c_str()), iRecurse);

	// DevMsg(eLog::NONE, "%s\n", szModuleName);

	// Call callbacks
	CallLoadLibraryACallbacks(szModuleName, hModule);
	CVar_InitModule(szModuleName);
}

//-----------------------------------------------------------------------------
// Load library callbacks

HMODULE WINAPI WLoadLibraryA(LPCSTR lpLibFileName)
{
	HMODULE hModule = o_LoadLibraryA(lpLibFileName);

	LibSys_RunModuleCallbacks(hModule);

	return hModule;
}

HMODULE WINAPI WLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE hModule;

	// replace xinput dll with one that has ASLR
	if (!strncmp(lpLibFileName, "XInput1_3.dll", 14))
	{
		hModule = o_LoadLibraryExA("XInput9_1_0.dll", hFile, dwFlags);

		if (!hModule)
		{
			Error(eLog::NS, EXIT_FAILURE, "Could not find XInput9_1_0.dll\n");

			return nullptr;
		}
	}
	else
	{
		hModule = o_LoadLibraryExA(lpLibFileName, hFile, dwFlags);
	}

	bool bShouldRunCalbacks = !(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
	if (bShouldRunCalbacks)
	{
		LibSys_RunModuleCallbacks(hModule);
	}

	return hModule;
}

HMODULE WINAPI WLoadLibraryW(LPCWSTR lpLibFileName)
{
	HMODULE hModule = o_LoadLibraryW(lpLibFileName);

	LibSys_RunModuleCallbacks(hModule);

	return hModule;
}

HMODULE WINAPI WLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	HMODULE hModule = o_LoadLibraryExW(lpLibFileName, hFile, dwFlags);

	bool bShouldRunCalbacks = !(dwFlags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
	if (bShouldRunCalbacks)
	{
		LibSys_RunModuleCallbacks(hModule);
	}

	return hModule;
}

//-----------------------------------------------------------------------------
// Purpose: Initilase dll load callbacks
//-----------------------------------------------------------------------------
void LibSys_Init()
{
	HMODULE hKernel = GetModuleHandleA("Kernel32.dll");

	o_LoadLibraryA = reinterpret_cast<ILoadLibraryA>(GetProcAddress(hKernel, "LoadLibraryA"));
	o_LoadLibraryExA = reinterpret_cast<ILoadLibraryExA>(GetProcAddress(hKernel, "LoadLibraryExA"));
	o_LoadLibraryW = reinterpret_cast<ILoadLibraryW>(GetProcAddress(hKernel, "LoadLibraryW"));
	o_LoadLibraryExW = reinterpret_cast<ILoadLibraryExW>(GetProcAddress(hKernel, "LoadLibraryExW"));

	HookAttach(&(PVOID&)o_LoadLibraryA, (PVOID)WLoadLibraryA);
	HookAttach(&(PVOID&)o_LoadLibraryExA, (PVOID)WLoadLibraryExA);
	HookAttach(&(PVOID&)o_LoadLibraryW, (PVOID)WLoadLibraryW);
	HookAttach(&(PVOID&)o_LoadLibraryExW, (PVOID)WLoadLibraryExW);
}
