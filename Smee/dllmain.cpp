// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <windows.h>
#include <CommCtrl.h>
#include <objbase.h>
#include <devpkey.h>
#include <stdio.h>
#include <Audioclient.h>
#include <detours.h>

#include <iostream>
#include <Psapi.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h> 
#include <initguid.h>
#include <vector>
#include <ShlDisp.h>
#include <shellapi.h>



#define _WIN32_DCOM
#define CINTERFACE

#define DEBUG_PRINTF(...) {char cad[512]; sprintf_s(cad, __VA_ARGS__);  OutputDebugStringA(cad);}

#define ID_SET 1
#define ID_DONATE 2

//Magic, may no longer be necessary

DEFINE_GUID(IID_IAudioClient,
            0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB1, 0x78,
            0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2
);

DEFINE_GUID(IID_IAudioRenderClient,
            0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf,
            0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2
);

///

const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

IMMDeviceEnumerator* baseEnumerator;

static LPCTSTR WindowClass = L"DeviceChooser";
HWND hBtn, hListbox, hDonate;
static bool escape = false;

IMMDevice* chosenDevice;

std::vector<IMMDevice*> deviceList;

///


static HRESULT(WINAPI* TrueCoCreateInstance)(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv);

HRESULT(STDMETHODCALLTYPE* TrueGetDefaultAudioEndpoint)(
    IMMDeviceEnumerator* instance,
    EDataFlow dataFlow,
    ERole role,
    IMMDevice** ppEndpoint
    );


///


void PopulateListbox(HWND hwnd, IMMDeviceEnumerator* enumerator) {
    IMMDeviceCollection* deviceCollection = NULL;

    HRESULT hr = enumerator->lpVtbl->EnumAudioEndpoints(
        enumerator,
        eRender, DEVICE_STATE_ACTIVE,
        &deviceCollection);

    IPropertyStore* pProps = NULL;
    deviceList.clear();

    UINT devCount;
    deviceCollection->lpVtbl->GetCount(deviceCollection, &devCount);
    DEBUG_PRINTF("Device count: %i\r\n", devCount);
    for (UINT i = 0; i < devCount; i++) {
        IMMDevice* device;
        deviceCollection->lpVtbl->Item(deviceCollection, i, &device);
        device->lpVtbl->OpenPropertyStore(device, STGM_READ, &pProps);

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        pProps->lpVtbl->GetValue(pProps, PKEY_Device_FriendlyName, &varName);
        
        int pos = (int) SendMessage(hwnd, LB_ADDSTRING, 0,
                                    (LPARAM) varName.pwszVal);
        SendMessage(hwnd, LB_SETITEMDATA, pos, (LPARAM) i);
        deviceList.push_back(device);

        PropVariantClear(&varName);


    }


}

LRESULT CALLBACK PromptCallback(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    switch (message) {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        case WM_CREATE:

            hBtn = CreateWindowExW(WS_EX_APPWINDOW,
                                   L"BUTTON", NULL,
                                   WS_CHILD | WS_VISIBLE,
                                   400, 0, 80, 21,
                                   hwnd, (HMENU) ID_SET, hInstance, NULL);

            SetWindowTextW(hBtn, L"&Set Device");

            hDonate = CreateWindowExW(WS_EX_APPWINDOW,
                                   L"BUTTON", NULL,
                                   WS_CHILD | WS_VISIBLE,
                                   400, 342, 80, 21,
                                   hwnd, (HMENU) ID_DONATE, hInstance, NULL);

            SetWindowTextW(hDonate, L"&Donate");

            hListbox = CreateWindowExW(WS_EX_CLIENTEDGE,
                                       L"LISTBOX", NULL,
                                       WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | LBS_NOTIFY,
                                       0, 0, 400, 400,
                                       hwnd, NULL, hInstance, NULL);
            break;
        case WM_COMMAND:
            if ((HIWORD(wParam) == BN_CLICKED) && (lParam != 0)) {
                if (LOWORD(wParam) == ID_SET) {
                    int choice = SendMessage(hListbox, LB_GETCURSEL, 0, 0);
                    DEBUG_PRINTF("choice: %i, size: %i\r\n", choice, deviceList.size());
                    if (choice > 0) {
                        chosenDevice = deviceList[choice];
                    }
                    escape = true;
                } else {
                    // :)
                    ShellExecute(NULL, L"open", L"https://www.paypal.com/paypalme/orderofthetilde", NULL, NULL, SW_SHOWNORMAL);
                }

                
            }
            
        default:
            return DefWindowProc(hwnd, message, wParam, lParam);
    }
    return 0;
}

void WINAPI DevicePrompt(IMMDeviceEnumerator* enumerator) {
    WNDCLASSEX wc = {0};
    HWND hwnd;
    MSG Msg;
    HINSTANCE hInstance = GetModuleHandleW(NULL);
    escape = false;


    //Step 1: Registering the Window Class
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = PromptCallback;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = WindowClass;
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wc)) {
        MessageBox(NULL, L"Window Registration Failed!", L"Error!",
                   MB_ICONEXCLAMATION | MB_OK);
        return;
    }


    hwnd = CreateWindowW(
        WindowClass,
        L"Device Chooser",
        WS_VISIBLE | WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        495,
        400,
        0,
        0,
        hInstance,
        0
    );



    if (hwnd == NULL) {
        MessageBox(NULL, L"Window Creation Failed!", L"Error!",
                   MB_ICONEXCLAMATION | MB_OK);
        return;
    }

    ShowWindow(hwnd, 1);
    UpdateWindow(hwnd);
    
    bool set = false;

    // Step 3: The Message Loop
    while (GetMessageW(&Msg, hwnd, 0, 0) > 0 && !escape) {
        TranslateMessage(&Msg);
        DispatchMessage(&Msg);
        if (hListbox && !set) {
            OutputDebugStringW(L"Populatin'\r\n");
            PopulateListbox(hListbox, enumerator);
            set = true;
        }
    }

    DestroyWindow(hwnd);
    CloseWindow(hwnd);
    DeleteObject(hBtn);
    DeleteObject(hListbox);
    DeleteObject(hDonate);
    ZeroMemory(&wc, sizeof(wc));

    return;
}


///








HRESULT GetDefaultAudioEndpoint(IMMDeviceEnumerator* instance, EDataFlow dataFlow, ERole role, IMMDevice** ppEndpoint) {
    OutputDebugString(L"Getting default audio endpoint.\r\n");
    HRESULT hr;
    LPWSTR ppstrId;
    if (dataFlow == EDataFlow::eRender) {
        DevicePrompt(instance);
        chosenDevice->lpVtbl->GetId(chosenDevice, &ppstrId);
        hr = instance->lpVtbl->GetDevice(instance, ppstrId, ppEndpoint);
    } else {
        hr = TrueGetDefaultAudioEndpoint(instance, dataFlow, role, ppEndpoint);
    }

    //if (SUCCEEDED(hr))
    //    HookEndpoint(*ppEndpoint);
    OutputDebugString(L"Default audio enpoint acquired.\r\n");
    return hr;
}

void HookEnumerator(IMMDeviceEnumerator* enumerator) {
    OutputDebugString(L"Starting hookenumerator.\r\n");

    static bool hooked = false;
    if (hooked)
        return;

    // Fetch original function
    baseEnumerator = enumerator;
    TrueGetDefaultAudioEndpoint = enumerator->lpVtbl->GetDefaultAudioEndpoint;

    // Hook enumerator functions
    DetourRestoreAfterWith();
    DetourTransactionBegin();
    DetourUpdateThread(GetCurrentThread());
    DetourAttach(&(PVOID&) TrueGetDefaultAudioEndpoint, GetDefaultAudioEndpoint);
    DetourTransactionCommit();
    OutputDebugString(L"Hookenumerator.\r\n");
    hooked = true;
}

HRESULT STDAPICALLTYPE  CoCreateInstanceHook(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID FAR* ppv) {
    OutputDebugString(L"CoCreateInstance.\r\n");

    if (rclsid == CLSID_MMDeviceEnumerator) {
        OutputDebugString(L"We at least got the hook here.\r\n");

        HRESULT hr = TrueCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
        HookEnumerator((IMMDeviceEnumerator*) *ppv);

        OutputDebugString(L"We at least got the hook (past) here.\r\n");
        return hr;
    }
    return TrueCoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);
}


BOOL APIENTRY DllMain(HMODULE hModule,
                      DWORD  ul_reason_for_call,
                      LPVOID lpReserved
) {
    LONG error;

    int dbg_reason = (int) ul_reason_for_call;

    DEBUG_PRINTF("Main Smee entry point accessed. Reason: %i\r\n", dbg_reason);

    //while (!::IsDebuggerPresent()) ::Sleep(100); // to avoid 100% CPU load

    switch (ul_reason_for_call) {
        case DLL_THREAD_ATTACH:
            OutputDebugString(L"(Thread) ");
        case DLL_PROCESS_ATTACH:
            OutputDebugString(L"Attaching smee.dll\r\n");
            TrueCoCreateInstance = CoCreateInstance;

            DetourRestoreAfterWith();
            DisableThreadLibraryCalls(hModule);
            DetourTransactionBegin();
            DetourUpdateThread(GetCurrentThread());
            DetourAttach(&(PVOID&) TrueCoCreateInstance, CoCreateInstanceHook);

            error = DetourTransactionCommit();


            if (error == NO_ERROR) {
                OutputDebugString(L"Hooking attempt succeeded!\r\n");
            } else {
                OutputDebugString(L"Hooking attempt failed\r\n");
            }
            break;
        case DLL_PROCESS_DETACH:
            OutputDebugString(L"Detaching HookingDLL.dll\r\n");
            OutputDebugString(L"Sike, i dont care.\r\n");

            break;
    }
    return TRUE;

}

extern "C" __declspec(dllexport) void dummy(void) {
    return;
}