#include <Windows.h>
#include <iostream>
#include <string>
#include <limits>

// Import functions from our DLL
typedef HRESULT (*StartInjectionFunc)(LPCWSTR deviceName, LPCWSTR filePath, float ratio);
typedef HRESULT (*CancelInjectionFunc)();

int main(int argc, char* argv[]) {
    // Load the DLL
    HMODULE hDll = LoadLibraryW(L"AudioInjectorClient.dll");
    if (!hDll) {
        std::cerr << "Failed to load AudioInjectorClient.dll. Error: " << GetLastError() << std::endl;
        return 1;
    }

    // Get function addresses
    StartInjectionFunc StartInjection = (StartInjectionFunc)GetProcAddress(hDll, "StartInjection");
    CancelInjectionFunc CancelInjection = (CancelInjectionFunc)GetProcAddress(hDll, "CancelInjection");

    if (!StartInjection || !CancelInjection) {
        std::cerr << "Failed to get function addresses from DLL" << std::endl;
        FreeLibrary(hDll);
        return 1;
    }

    // Simple menu
    int choice = 0;
    std::wstring deviceName;
    std::wstring filePath;
    float ratio = 0.5f;
    HRESULT hr = S_OK;

    do {
        std::cout << "\nAudio Injector Client Test Application\n";
        std::cout << "-------------------------------------\n";
        std::cout << "1. Start audio injection\n";
        std::cout << "2. Cancel audio injection\n";
        std::cout << "3. Exit\n";
        std::cout << "Choice: ";
        std::cin >> choice;

        switch (choice) {
        case 1:            // Clear input buffer
            std::cin.ignore(10000, '\n');

            // Get device name (optional)
            std::cout << "Enter audio device name (leave blank for default): ";
            std::getline(std::wcin, deviceName);

            // Get file path
            std::cout << "Enter audio file path: ";
            std::getline(std::wcin, filePath);

            // Get mix ratio
            std::cout << "Enter mix ratio (0.0-1.0): ";
            std::cin >> ratio;

            // Start injection
            hr = StartInjection(
                deviceName.empty() ? nullptr : deviceName.c_str(),
                filePath.c_str(),
                ratio);

            if (SUCCEEDED(hr)) {
                std::cout << "Audio injection started successfully" << std::endl;
            } else {                wchar_t errorMsg[256] = {0};
                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, errorMsg, 256, nullptr);
                std::wcerr << L"Failed to start audio injection. HRESULT: 0x"
                          << std::hex << hr << std::dec
                          << L" - " << errorMsg << std::endl;
            }
            break;

        case 2:
            hr = CancelInjection();
            if (SUCCEEDED(hr)) {
                std::cout << "Audio injection cancelled successfully" << std::endl;
            } else {                wchar_t errorMsg[256] = {0};
                FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, hr, 0, errorMsg, 256, nullptr);
                std::wcerr << L"Failed to cancel audio injection. HRESULT: 0x"
                          << std::hex << hr << std::dec
                          << L" - " << errorMsg << std::endl;
            }
            break;

        case 3:
            std::cout << "Exiting...\n";
            break;

        default:
            std::cout << "Invalid choice\n";
            break;
        }
    } while (choice != 3);

    // Free the DLL
    FreeLibrary(hDll);
    return 0;
}
