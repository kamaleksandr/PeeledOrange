# 🍊 PeeledOrange

**PeeledOrange** — высокопроизводительный асинхронный движок для создания EAV-систем (Entity-Attribute-Value) на C++14 и Java.

Разработан для highload-проектов, где критичны скорость обработки запросов, эффективная работа с сокетами и асинхронный доступ к базам данных.

## 📦 Состав

- **`engine/`** — C++14 библиотека: асинхронный epoll-сервер, TCP-соединения, буферизация, таймауты, опциональный TLS, абстрактный DB-клиент.
- **`client/`** — Java-клиент: бинарный протокол, сериализация EAV-модели (EavTask, Attribute, EntityL).
- **`examples/`** — готовые примеры использования (echo-сервер, демо с асинхронными callback БД).

## 🚀 Возможности

- **Асинхронный I/O** на epoll (один поток, неблокирующие сокеты).
- **Собственный бинарный протокол** с CRC16, переменной длиной полей, поддержкой фрагментации.
- **Поддержка TLS/SSL** (опционально, через `-DENABLE_TLS=ON`).
- **Абстрактный DB-клиент** — легко подключить PostgreSQL или любую другую БД (пример в `demo_db.cpp`).
- **Проверка целостности данных**: CRC для заголовков и EAV-данных (до 64 КБ).
- **Готовые нагрузочные тесты** (Java).

## 📊 Результаты нагрузочного тестирования

Тесты проводились на сервере: 4 vCPU, 4.5 ГБ RAM, AlmaLinux 8.  
PostgreSQL с настройками по умолчанию (синхронный коммит). Клиент — Windows 10, 40-70 потоков, домашняя сеть 500Мбит/с.

| Тип нагрузки | RPS (стабильно) | Ошибки | Латентность |
|--------------|-----------------|--------|--------------|
| **PING** (пустой запрос) | **1M+** | 0% | **0.04 мс** |
| **READ_ATTRIBUTE** (чтение из БД) | **255k** | 0.02% | **0.13 мс** |
| **WRITE_ATTRIBUTE** (запись в БД) | **297k** | 0.03% | **0.13 мс** |

> Тесты проводились на стандартном облачном VPS с HDD-диском, что стало основным ограничением производительности записи (300k RPS).
> При переходе на SSD (или NVMe) можно ожидать роста RPS в 5–10 раз, до 1.5–3 млн операций записи в секунду.

## 🔧 Сборка (C++)

### Зависимости (AlmaLinux / CentOS / Fedora)

```bash
sudo yum groupinstall "Development Tools"
sudo yum install cmake openssl-devel postgresql-devel
Сборка библиотеки и примеров
bash
git clone https://github.com/kamaleksandr/PeeledOrange.git
cd PeeledOrange
mkdir build && cd build
cmake -DENABLE_TLS=ON -DENABLE_DATABASE=ON -DBUILD_DEMO=ON ..
make
Опции CMake
Опция	По умолчанию	Описание
ENABLE_DEBUG	OFF	Включить отладочные логи (DEBUG_MODE)
ENABLE_TLS	OFF	Включить поддержку TLS/SSL (TLS_MODE)
ENABLE_DATABASE	OFF	Включить поддержку БД (DATABASE_CLIENT)
BUILD_DEMO	OFF	Собрать демо-сервер (demo_db.cpp)
🏃 Запуск примеров
Echo-сервер (без БД)
bash
./echo_server
Демо с асинхронной БД
bash
./demo_server
При подключении к БД вызывается afterDBConnect(), результат запроса приходит в onDBResult().

☕ Java клиент
Сборка (в папке client/):

bash
javac *.java
java DemoClient
Пример создания задачи:

java
EavTask task = EavTask.createPing();
ByteBuffer buffer = task.getRequestData();
System.out.println("Packet size: " + buffer.capacity());
📜 Лицензия
MIT (См. файл LICENSE)

Peeled Orange — полноценный промышленный движок для highload EAV‑систем, проверенный на нагрузках и готовый к использованию.

🍊 Автор
Александр Камышев (@kamaleksandr)

PeeledOrange — потому что хороший код должен быть чистым, как очищенный апельсин.
