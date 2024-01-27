#include <Windows.h>
#include <iostream>

//получаем аргументы запуска для последующей передачи дочернему процессу
std::wstring GetCommandLineArguments()
{
	LPWSTR* argv;
	int argc;

	argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	std::wstring arguments;

	for (int i = 1; i < argc; i++)
	{
		arguments += argv[i];
		arguments += L" ";
	}

	LocalFree(argv);

	return arguments;
}

int main(int argc, wchar_t* argv[])
{

	// Получение пути к текущему исполняемому файлу
	char modulePath[MAX_PATH];
	GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
	// Получение пути к каталогу текущего исполняемого файла
	std::string moduleDirectory = modulePath;
	size_t lastBackslashIndex = moduleDirectory.find_last_of("\\");
	std::string workingDirectory = moduleDirectory.substr(0, lastBackslashIndex);
	// Путь к исполняемому файлу приложения
	std::string applicationPath = workingDirectory + "\\PalServer-Win64-Test-Cmd.exe";
	const char* applicationPathCStr = applicationPath.c_str();
	// Путь к DLL-файлу для инжекции
	const char* dllPath = "UE4SS.dll";
	//Аргументы запуска
	std::string commandLine = "Pal";
	std::wstring additionalArgs = GetCommandLineArguments();
	commandLine += " " + std::string(additionalArgs.begin(), additionalArgs.end());

	// Запуск приложения
	STARTUPINFOA startupInfo = { sizeof(STARTUPINFOA) };
	PROCESS_INFORMATION processInfo;
	
	printf("Create Process, trying to inject dll...\n");
  //if (!CreateProcessA("PalServer-Win64-Test-Cmd.exe", const_cast<char*>(commandLine.c_str()), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, si, pi);
	if (!CreateProcessA(applicationPathCStr, const_cast<char*>(commandLine.c_str()), nullptr, nullptr, FALSE, 0, nullptr, workingDirectory.c_str(), &startupInfo, &processInfo))
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
