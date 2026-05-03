#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define SERVER_PORT 12345
#define WM_SOCKET (WM_USER + 1)

#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <fstream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;

SOCKET g_listenSocket = INVALID_SOCKET;
SOCKET g_clientSocket = INVALID_SOCKET;

ofstream g_outFile;
string g_outputFileName = "received_async.rtf";

char g_buffer[1024];

void OpenReceivedFile()
{
    ShellExecuteA(NULL, "open", g_outputFileName.c_str(), NULL, NULL, SW_SHOWNORMAL);
}

void CleanupClient()
{
    if (g_clientSocket != INVALID_SOCKET)
    {
        closesocket(g_clientSocket);
        g_clientSocket = INVALID_SOCKET;
    }

    if (g_outFile.is_open())
    {
        g_outFile.close();
    }
}

bool StartServer(HWND hWnd)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        MessageBoxA(hWnd, "WSAStartup failed", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    g_listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listenSocket == INVALID_SOCKET)
    {
        MessageBoxA(hWnd, "socket() failed", "Error", MB_OK | MB_ICONERROR);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        MessageBoxA(hWnd, "bind() failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        return false;
    }

    if (listen(g_listenSocket, 1) == SOCKET_ERROR)
    {
        MessageBoxA(hWnd, "listen() failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        return false;
    }

    // Реєстрація сокета для отримання повідомлень про події
    if (WSAAsyncSelect(g_listenSocket, hWnd, WM_SOCKET, FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
    {
        MessageBoxA(hWnd, "WSAAsyncSelect() failed", "Error", MB_OK | MB_ICONERROR);
        closesocket(g_listenSocket);
        g_listenSocket = INVALID_SOCKET;
        return false;
    }

    MessageBoxA(hWnd, "Async server started on port 12345", "Info", MB_OK | MB_ICONINFORMATION);
    return true;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        StartServer(hWnd);
        return 0;
    }

    case WM_SOCKET:
    {
        SOCKET currentSocket = (SOCKET)wParam;
        int eventCode = WSAGETSELECTEVENT(lParam);
        int errorCode = WSAGETSELECTERROR(lParam);

        if (errorCode != 0)
        {
            MessageBoxA(hWnd, "Socket event error", "Error", MB_OK | MB_ICONERROR);
            return 0;
        }

        if (eventCode == FD_ACCEPT)
        {
            sockaddr_in clientAddr{};
            int clientLen = sizeof(clientAddr);

            g_clientSocket = accept(g_listenSocket, (sockaddr*)&clientAddr, &clientLen);
            if (g_clientSocket == INVALID_SOCKET)
            {
                MessageBoxA(hWnd, "accept() failed", "Error", MB_OK | MB_ICONERROR);
                return 0;
            }

            // Для клієнтського сокета вмикаємо асинхронні події читання і закриття
            if (WSAAsyncSelect(g_clientSocket, hWnd, WM_SOCKET, FD_READ | FD_CLOSE) == SOCKET_ERROR)
            {
                MessageBoxA(hWnd, "WSAAsyncSelect() for client failed", "Error", MB_OK | MB_ICONERROR);
                closesocket(g_clientSocket);
                g_clientSocket = INVALID_SOCKET;
                return 0;
            }

            if (g_outFile.is_open())
                g_outFile.close();

            g_outFile.open(g_outputFileName, std::ios::binary);
            if (!g_outFile.is_open())
            {
                MessageBoxA(hWnd, "Cannot create output file", "Error", MB_OK | MB_ICONERROR);
                CleanupClient();
                return 0;
            }

            SetWindowTextA(hWnd, "AsyncServer - Client connected");
        }
        else if (eventCode == FD_READ)
        {
            if (currentSocket == g_clientSocket && g_outFile.is_open())
            {
                int bytesReceived = recv(g_clientSocket, g_buffer, sizeof(g_buffer), 0);

                if (bytesReceived > 0)
                {
                    g_outFile.write(g_buffer, bytesReceived);
                }
            }
        }
        else if (eventCode == FD_CLOSE)
        {
            if (currentSocket == g_clientSocket)
            {
                CleanupClient();
                SetWindowTextA(hWnd, "AsyncServer - File received");
                MessageBoxA(hWnd, "File received successfully", "Info", MB_OK | MB_ICONINFORMATION);
                OpenReceivedFile();
            }
        }

        return 0;
    }

    case WM_DESTROY:
    {
        CleanupClient();

        if (g_listenSocket != INVALID_SOCKET)
        {
            closesocket(g_listenSocket);
            g_listenSocket = INVALID_SOCKET;
        }

        WSACleanup();
        PostQuitMessage(0);
        return 0;
    }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    const char CLASS_NAME[] = "AsyncServerWindowClass";

    WNDCLASSA wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassA(&wc);

    HWND hWnd = CreateWindowA(
        CLASS_NAME,
        "AsyncServer - Waiting",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 300,
        NULL, NULL, hInstance, NULL
    );

    if (hWnd == NULL)
        return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg{};
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}