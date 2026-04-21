#include <windows.h>
#include <winscard.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winscard.lib")

// --- GUI COMPONENTS ---
HWND hAidInput, hRidInput, hApduBox, hLogOutput;
SCARDCONTEXT hContext;
SCARDHANDLE hCard;
bool bConnected = false;

// --- HEX CONVERSION ENGINE ---
std::vector<BYTE> HexToBytes(const std::string& hex) {
    std::vector<BYTE> bytes;
    std::string cleanHex;
    for (char c : hex) if (isxdigit(c)) cleanHex += c;
    for (size_t i = 0; i < cleanHex.length(); i += 2) {
        bytes.push_back((BYTE)strtol(cleanHex.substr(i, 2).c_str(), NULL, 16));
    }
    return bytes;
}

void Log(const std::string& msg) {
    int len = GetWindowTextLength(hLogOutput);
    SendMessage(hLogOutput, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessage(hLogOutput, EM_REPLACESEL, 0, (LPARAM)(msg + "\r\n").c_str());
}

// --- HYPERX TRANSMISSION ENGINE ---
void RunSequence() {
    if (!bConnected) {
        LPSTR mszReaders = NULL;
        DWORD dwReaders = SCARD_AUTOALLOCATE;
        if (SCardListReadersA(hContext, NULL, (LPSTR)&mszReaders, &dwReaders) != SCARD_S_SUCCESS) {
            Log("[!] NO READER DETECTED");
            return;
        }
        if (SCardConnectA(hContext, mszReaders, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T1, &hCard, &dwReaders) != SCARD_S_SUCCESS) {
            Log("[!] CONNECTION FAILED");
            return;
        }
        bConnected = true;
        Log("[+] DIRECTX BRIDGE ESTABLISHED");
    }

    char buffer[4096];
    GetWindowText(hApduBox, buffer, 4096);
    std::stringstream ss(buffer);
    std::string line;

    while (std::getline(ss, line)) {
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos) continue;

        std::vector<BYTE> cmd = HexToBytes(line);
        if (cmd.empty()) continue;

        BYTE pbRecv[256];
        DWORD dwRecv = sizeof(pbRecv);
        LONG rv = SCardTransmit(hCard, SCARD_PCI_T1, cmd.data(), (DWORD)cmd.size(), NULL, pbRecv, &dwRecv);

        std::stringstream logEntry;
        logEntry << "TX: " << line.substr(0, 15) << "... | RX: ";
        if (rv == SCARD_S_SUCCESS) {
            for (DWORD i = 0; i < dwRecv; i++) 
                logEntry << std::hex << std::setw(2) << std::setfill('0') << (int)pbRecv[i];
        } else {
            logEntry << "ERROR " << std::hex << rv;
        }
        Log(logEntry.str());
    }
}

// --- WINDOW PROCEDURE ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:
        // Labels & Inputs
        CreateWindow("STATIC", "TARGET AID:", WS_CHILD | WS_VISIBLE, 10, 15, 80, 20, hwnd, NULL, NULL, NULL);
        hAidInput = CreateWindow("EDIT", "A0 00 00 00 04 10 10", WS_CHILD | WS_VISIBLE | WS_BORDER, 100, 12, 200, 20, hwnd, NULL, NULL, NULL);

        CreateWindow("STATIC", "TARGET RID:", WS_CHILD | WS_VISIBLE, 10, 40, 80, 20, hwnd, NULL, NULL, NULL);
        hRidInput = CreateWindow("EDIT", "A0 00 00 00 04", WS_CHILD | WS_VISIBLE | WS_BORDER, 100, 37, 200, 20, hwnd, NULL, NULL, NULL);

        // APDU Sequence Box (30 Lines)
        CreateWindow("STATIC", "APDU HYPERX COMMANDS:", WS_CHILD | WS_VISIBLE, 10, 70, 200, 20, hwnd, NULL, NULL, NULL);
        hApduBox = CreateWindow("EDIT", "00 A4 04 00 00\r\n80 50 00 00 08 01 02 03 04 05 06 07 08\r\n84 82 00 00 10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00\r\n80 E4 00 80 09 4F 07 A0 00 00 00 04 10 10\r\n80 E6 02 00 14 07 A0 00 00 00 04 10 10 08 A0 00 00 00 04 10 10 00 00 00\r\n80 E8 00 00 FF\r\n80 E8 00 01 FF\r\n80 E8 80 02 4F\r\n80 E6 0C 00 22 07 A0 00 00 00 04 10 10 07 A0 00 00 00 04 10 10 07 A0 00 00 00 04 10 10 01 00 02 C9 00 00 00", 
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | WS_BORDER, 10, 90, 460, 250, hwnd, NULL, NULL, NULL);

        // Run Button
        CreateWindow("BUTTON", "EXECUTE REX", WS_CHILD | WS_VISIBLE, 320, 12, 150, 45, hwnd, (HMENU)1, NULL, NULL);

        // Log
        hLogOutput = CreateWindow("EDIT", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | WS_BORDER, 10, 350, 460, 150, hwnd, NULL, NULL, NULL);

        SCardEstablishContext(SCARD_SCOPE_SYSTEM, NULL, NULL, &hContext);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == 1) RunSequence();
        break;

    case WM_DESTROY:
        if (bConnected) SCardDisconnect(hCard, SCARD_LEAVE_CARD);
        SCardReleaseContext(hContext);
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInst;
    wc.lpszClassName = "JCOPREX";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClass(&wc);
    HWND hwnd = CreateWindow("JCOPREX", "JCOP REX HEX : OMNIBUS DIRECTX", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 500, 560, NULL, NULL, hInst, NULL);
    ShowWindow(hwnd, nShow);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    return 0;
}