// enabled ready up code block

#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

int client_socket;

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            cout << "\nDisconnected from server." << endl;
            close(client_socket);
            exit(0);
        }

        cout << buffer << endl;
    }
    return nullptr;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t recv_thread;
    
    cout << "Connecting...\n";

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        cerr << "Socket creation failed." << endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        cerr << "Connection to server failed." << endl;
        return 1;
    }

    cout << "Connected to server!\nEnter your name: ";
    string name;
    getline(cin, name);
    while (name.empty()) {
        cout << "Name can't be empty. Try again: ";
        getline(cin, name);
    }
    send(client_socket, name.c_str(), name.length(), 0);

    // Ready up
    cout << "[Press 1 to ready up!]" << endl;
    string ready;
    getline(cin, ready);
    send(client_socket, ready.c_str(), ready.length(), 0);

    // Start receiver thread
    pthread_create(&recv_thread, nullptr, receive_messages, nullptr);
    pthread_detach(recv_thread);

    // Main input loop (optional additional commands)
    while (true) {
        string input;
        getline(cin, input);

        if (input == "exit") {
            send(client_socket, input.c_str(), input.length(), 0);
            break;
        }

        // optional: send manual input
        send(client_socket, input.c_str(), input.length(), 0);
    }

    close(client_socket);
    return 0;
}

