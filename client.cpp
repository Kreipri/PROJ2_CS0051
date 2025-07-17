#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 1024

int client_socket;

void* receive_messages(void* arg) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            std::cout << "\nDisconnected from server." << std::endl;
            close(client_socket);
            exit(0);
        }

        std::cout << buffer << std::endl;
    }
    return nullptr;
}

int main() {
    struct sockaddr_in server_addr;
    pthread_t recv_thread;
    
    std::cout<<"Connecting...\n";

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        std::cerr << "Socket creation failed." << std::endl;
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        std::cerr << "Connection to server failed." << std::endl;
        return 1;
    }

    std::cout << "Connected to server!\nEnter your name: ";
    std::string name;
    std::getline(std::cin, name);
    while (name.empty()) {
        std::cout << "Name can't be empty. Try again: ";
        std::getline(std::cin, name);
    }
    send(client_socket, name.c_str(), name.length(), 0);

    // Ready up
    //std::cout << "[Press 1 to ready up!]" << std::endl;
    //std::string ready;
    //std::getline(std::cin, ready);
    //send(client_socket, ready.c_str(), ready.length(), 0);

    // Start receiver thread
    pthread_create(&recv_thread, nullptr, receive_messages, nullptr);
    pthread_detach(recv_thread);

    // Main input loop (optional additional commands)
    while (true) {
        std::string input;
        std::getline(std::cin, input);

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

