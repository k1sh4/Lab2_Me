#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <fstream>
#include <vector>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cout << "WSAStartup failed\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == INVALID_SOCKET) {
        cout << "socket() failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(12345); // Порт збігається з сервером

    if (InetPtonA(AF_INET, "127.0.0.1", &serverAddr.sin_addr) != 1) {
        cout << "Invalid IP address\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cout << "connect() failed. Перевір, чи запущено Сервер!\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    ifstream inFile("test.rtf", ios::binary);
    if (!inFile.is_open()) {
        cout << "Cannot open test.rtf. Перевір, чи лежить файл поруч з програмою!\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    vector<char> fileData((istreambuf_iterator<char>(inFile)), istreambuf_iterator<char>());
    inFile.close();

    int parts = 47; // Твій варіант
    if (parts <= 0) parts = 1;

    int totalSize = (int)fileData.size();
    int partSize = totalSize / parts;
    int remainder = totalSize % parts;

    int offset = 0;

    for (int i = 0; i < parts; i++) {
        int currentPartSize = partSize;

        if (i == parts - 1)
            currentPartSize += remainder;

        if (currentPartSize > 0) {
            int sent = send(clientSocket, fileData.data() + offset, currentPartSize, 0);
            if (sent == SOCKET_ERROR) {
                cout << "send() failed on fragment " << i + 1 << endl;
                closesocket(clientSocket);
                WSACleanup();
                return 1;
            }

            cout << "Fragment " << i + 1 << " sent, size = " << currentPartSize << " bytes\n";
            offset += currentPartSize;
        }
    }

    closesocket(clientSocket);
    WSACleanup();

    cout << "All fragments sent successfully\n";
    return 0;
}