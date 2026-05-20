/**
 * @file echo_server.cpp
 * @brief Простой echo-сервер на основе TCPServer.
 * 
 * Пример использования библиотеки peeled_engine без БД и TLS.
 * Сервер принимает соединения и отправляет обратно всё, что получил.
 */

#include "highload/tcp_server.h"
#include <cstdio>

class EchoServer : public highload::TCPServer {
public:
    EchoServer() {
        // Без БД, без TLS
    }
    
    void OnReadData(types::context_data_t cx) override {
        // Отправляем клиенту данные, которые только что получили
        int32_t sent = SendData(cx.connection, 
                                 cx.connection->input.data.get(), 
                                 cx.connection->input.size);
        if (sent > 0) {
            printf("[EchoServer] Echoed %d bytes to client %d\n", 
                   sent, cx.connection->sock_fd);
        }
    }
    
    void OnConnect(types::context_data_t &cx) override {
        printf("[EchoServer] Client %d connected\n", cx.connection->sock_fd);
    }
    
    void OnDisconnect(types::context_data_t &cx) override {
        printf("[EchoServer] Client %d disconnected\n", cx.connection->sock_fd);
    }
};

int main() {
    EchoServer server;
    
    // Открываем сервер с конфигурацией из файла echo_config.cfg
    if (!server.Open("echo_config")) {
        fprintf(stderr, "Failed to open server\n");
        return 1;
    }
    
    printf("Echo server started on port 6999. Press Enter to stop.\n");
    server.Start();
    getchar();  // ждём нажатия Enter
    server.Close();
    
    return 0;
}