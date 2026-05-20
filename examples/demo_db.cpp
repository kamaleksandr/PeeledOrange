#include "highload/tcp_server.h"

#ifdef DATABASE_CLIENT
#include "database/db_client.h"

// Глобальный указатель на сервер (для callback'ов)
// В полноценном проекте лучше передавать контекст через user_data
static class DemoServer* g_server = nullptr;

// Пример callback-класса
class DemoDbClient : public database::TAbstractDbClient {
public:
    void OnTimer(uint32_t now) override {
        printf("[DemoDbClient] OnTimer: %u\n", now);
    }
    
    bool Query(std::unique_ptr<database::query_data_t> query) override {
        printf("[DemoDbClient] Query called, command: %d\n", query->command);
        // Здесь можно отправить запрос в PostgreSQL
        return true;
    }
    
    bool DataProcessing(int32_t fd) override {
        // Обработка событий от сокета БД (например, libpq)
        return false;
    }
};
#endif

class DemoServer : public highload::TCPServer {
public:
    DemoServer() {
#ifdef DATABASE_CLIENT
        db_client = new DemoDbClient();
        g_server = this;  // для callback'ов
        
        // Устанавливаем callback'и
        db_client->AfterConnect = [](int32_t fd) {
            if (g_server) g_server->afterDBConnect(fd);
        };
        
        db_client->OnResult = [](database::p_query_t pqd) {
            if (g_server) {
                task_t* task = (task_t*)pqd.get();
                g_server->onDBResult(*task);
            }
        };
#endif
    }
    
    ~DemoServer() {
#ifdef DATABASE_CLIENT
        delete db_client;
        g_server = nullptr;
#endif
    }
    
    // Методы, которые будут вызваны через callback'и
    void afterDBConnect(int32_t fd) {
        printf("[DemoServer] Database connected (fd=%d)\n", fd);
    }
    
    void onDBResult(task_t& task) {
        printf("[DemoServer] Database query result received\n");
        // Здесь можно обработать результат и отправить ответ клиенту
    }
    
    void OnReadData(types::context_data_t cx) override {
        // Эхо-ответ (для теста)
        SendData(cx.connection, cx.connection->input.data.get(), cx.connection->input.size);
        
        // Пример запроса к БД (если нужно)
        // std::unique_ptr<database::query_data_t> query = std::make_unique<task_t>();
        // db_client->Query(std::move(query));
    }
};

int main() {
    DemoServer server;
    if (!server.Open("demo_config")) {
        return 1;
    }
    printf("Demo server started. Press Enter to stop.\n");
    server.Start();
    getchar(); // ждём Enter
    server.Close();
    return 0;
}