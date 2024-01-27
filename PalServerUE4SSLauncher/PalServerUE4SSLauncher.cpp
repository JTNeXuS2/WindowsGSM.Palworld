#include <Windows.h>
#include <iostream>
#include <string>

int wmain(int argc, wchar_t* argv[])
{
	// Путь к DLL-файлу для инжекции
	const char* dllPath = "UE4SS.dll";
	// Получаем все аргументы при запуске
	std::wstring arguments;
	for (int i = 1; i < argc; i++)
	{
		arguments += argv[i];
		arguments += L" ";
	}
	//Добавляем аргументы запуска
	std::wstring commandLine = L"PalServer-Win64-Test-Cmd.exe Pal " + arguments;

	// Запуск приложения
	STARTUPINFOW startupInfo{};
	PROCESS_INFORMATION processInfo{};
	printf("Create Process, trying to inject dll...\n");
	if (!CreateProcessW(NULL, const_cast<LPWSTR>(commandLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &startupInfo, &processInfo))
	{
		std::cout << "Failed to launch application" << std::endl;
		return 1;
	}

	// Ожидание некоторого времени, чтобы приложение успело инициализироваться
	Sleep(2000);

	// Открытие процесса
	HANDLE processHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processInfo.dwProcessId);
	if (!processHandle)
	{
		std::cout << "Failed to open process" << std::endl;
		return 1;
	}

	// Выделение памяти в процессе для пути к DLL-файлу
	LPVOID dllPathAddress = VirtualAllocEx(processHandle, nullptr, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	if (!dllPathAddress)
	{
		std::cout << "Failed to allocate memory in remote process" << std::endl;
		CloseHandle(processHandle);
		return 1;
	}

	// Запись пути к DLL-файлу в выделенную память
	if (!WriteProcessMemory(processHandle, dllPathAddress, dllPath, strlen(dllPath) + 1, nullptr))
	{
		std::cout << "Failed to write to remote process memory" << std::endl;
		VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
		CloseHandle(processHandle);
		return 1;
	}

	// Получение адреса функции LoadLibraryA в ядре процесса
	HMODULE kernel32Module = GetModuleHandleA("kernel32.dll");
	FARPROC loadLibraryAddress = GetProcAddress(kernel32Module, "LoadLibraryA");

	// Создание удаленного потока для вызова LoadLibraryA с путем к DLL-файлу в качестве аргумента
	HANDLE thread = CreateRemoteThread(processInfo.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, dllPathAddress, 0, NULL);
	if (thread) {
		WaitForSingleObject(thread, 5000);
		VirtualFreeEx(processInfo.hProcess, dllPathAddress, 0, MEM_RELEASE);
		printf("DLL Injected");

		ResumeThread(processInfo.hThread);

		printf("Server Started!");

		WaitForSingleObject(processInfo.hProcess, INFINITE);
		CloseHandle(thread);

		DWORD exitCode;
		if (GetExitCodeProcess(processInfo.hProcess, &exitCode) && exitCode != STILL_ACTIVE)
		{
			CloseHandle(processHandle);
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);
			std::cout << "Child process exited with code: " << exitCode << std::endl;
			return exitCode;
		}
		else
		{
			TerminateProcess(processInfo.hProcess, 1);
			CloseHandle(processHandle);
			CloseHandle(processInfo.hThread);
			CloseHandle(processInfo.hProcess);
			return 1;
		}
	}
	else {
		printf("Failed To Create Thread");
		VirtualFreeEx(processInfo.hProcess, dllPathAddress, 0, MEM_RELEASE);
	}

	// Освобождение выделенной памяти и закрытие дескрипторов
	VirtualFreeEx(processHandle, dllPathAddress, 0, MEM_RELEASE);
	CloseHandle(thread);
	CloseHandle(processHandle);
	CloseHandle(processInfo.hThread);
	CloseHandle(processInfo.hProcess);
	free(processInfo.hThread);
	free(processInfo.hProcess);
	fflush(stdout);

	std::cout << "Injection done" << std::endl;
	return EXIT_SUCCESS;
	//return 0;
}
