#include <iostream>
#include <windows.h>
#include <detours.h>
#include <string>
#include <Atlconv.h>
#include <string>
#include <stringapiset.h>
#include <commctrl.h>
#pragma warning(push)
#if _MSC_VER > 1400
#pragma warning(disable:6102 6103) // /analyze warnings
#endif
#include <strsafe.h>
#pragma warning(pop)


std::wstring ConvertStringToWstring(const std::string& str) { // https://stackoverflow.com/a/44777607
    if (str.empty())     {
        return std::wstring();
    }
    int num_chars = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), NULL, 0);
    std::wstring wstrTo;
    if (num_chars)     {
        wstrTo.resize(num_chars);
        if (MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, str.c_str(), str.length(), &wstrTo[0], num_chars))         {
            return wstrTo;
        }
    }
    return std::wstring();
}

std::string GetCurrentDirectory() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");

    return std::string(buffer).substr(0, pos);
}

int CDECL main(int argc, char* argv[])
{
    if (argc == 0)
    {
        std::cout << "Usage: Captain Hook.exe [executable]" << std::endl;
        return 0;
    }

    USES_CONVERSION;
    USES_CONVERSION_EX;


    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);

    std::wstring arguments = L"";
    for (int i = 1; i < argc; i++) {
        arguments.append(ConvertStringToWstring(argv[i]));
        arguments.append(L" ");
    }

    
    #ifdef _WIN64
    std::string dllPath = GetCurrentDirectory() + "\\Smeex64.dll";
    #else
    std::string dllPath = GetCurrentDirectory() + "\\Smee.dll";
    #endif
    

    BOOL status = DetourCreateProcessWithDllExW(NULL, // lpApplicationName
                                                const_cast<LPWSTR>(arguments.c_str()), // lpCommandLine
                                                NULL, //lpProcessAttributes
                                                NULL, //lpThreadAttributes
                                                TRUE, //bInheritHandles
                                                CREATE_DEFAULT_ERROR_MODE, //dwCreationFlags
                                                NULL, //lpEnvironment
                                                NULL, //lpCurrentDirectory
                                                &si, //lpStartupInfo
                                                &pi, //lpProcessInformation
                                                dllPath.c_str(), //lpDllName
                                                NULL); // pfCreateProcessW



    if (status) {
        OutputDebugString(L"Process started successfully\r\n");
        std::cout << "Process started successfully!" << std::endl;
        std::cout << "Press enter to close...";

        std::cin.get();
    } else {
        OutputDebugString(L"Process errored!\r\n");
        std::cout << "Process errored!\r\nCommand line: ";
        std::wcout << arguments << std::endl;
        std::cout << "Press enter to close...";

        std::cin.get();
        return 1;

    }

    return 0;
    
}

