cmake_minimum_required(VERSION 3.14)
project(peeled_engine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Опции сборки (по умолчанию ВЫКЛЮЧЕНЫ)
option(ENABLE_DEBUG "Enable debug mode (DEBUG_MODE)" OFF)
option(ENABLE_TLS "Enable TLS/SSL support (TLS_MODE)" OFF)
option(ENABLE_DATABASE "Enable database client (DATABASE_CLIENT)" OFF)
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_DEMO "Build demo server" OFF)

# --- Ищем зависимости (только если они нужны) ---
if(ENABLE_TLS)
    find_package(OpenSSL REQUIRED)
endif()

if(ENABLE_DATABASE)
    find_package(PostgreSQL REQUIRED)  # Ищет libpq [citation:1]
endif()

# --- Собираем библиотеку движка ---
add_library(peeled_engine
    src/multiplexer.cpp
    src/tcp_server.cpp
    src/types.cpp
    src/db_client.cpp
)

target_include_directories(peeled_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Флаги препроцессора и системные библиотеки
if(ENABLE_DEBUG)
    target_compile_definitions(peeled_engine PRIVATE DEBUG_MODE)
endif()

if(ENABLE_TLS)
    target_compile_definitions(peeled_engine PRIVATE TLS_MODE)
    target_link_libraries(peeled_engine PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(ENABLE_DATABASE)
    target_compile_definitions(peeled_engine PRIVATE DATABASE_CLIENT)
    target_link_libraries(peeled_engine PRIVATE PostgreSQL::PostgreSQL)
endif()

# Обязательные системные библиотеки (pthread, dl, rt)
# Это правильный CMake-способ, он сам подставит -pthread и -ldl [citation:2][citation:6]
find_package(Threads REQUIRED)
target_link_libraries(peeled_engine
    PRIVATE
        Threads::Threads
        ${CMAKE_DL_LIBS}
        rt
)

# --- Устанавливаем библиотеку ---
include(GNUInstallDirs)
install(TARGETS peeled_engine
    EXPORT peeled_engineTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Экспортируем конфигурацию для find_package
export(EXPORT peeled_engineTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/peeled_engineTargets.cmake"
)

# --- Пример (опционально, если есть examples/echo_server.cpp) ---
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/examples/echo_server.cpp)
    add_executable(echo_server examples/echo_server.cpp)
    target_link_libraries(echo_server PRIVATE peeled_engine)
endif()

if(BUILD_DEMO AND ENABLE_DATABASE)
    add_executable(demo_server examples/demo_db.cpp)
    target_link_libraries(demo_server PRIVATE peeled_engine)
    target_compile_definitions(demo_server PRIVATE DATABASE_CLIENT)
endif()