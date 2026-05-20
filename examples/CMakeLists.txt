add_executable(echo_server echo_server.cpp)
target_link_libraries(echo_server PRIVATE peeled_engine)

if(ENABLE_DATABASE)
    add_executable(demo_server demo_db.cpp)
    target_link_libraries(demo_server PRIVATE peeled_engine)
    target_compile_definitions(demo_server PRIVATE DATABASE_CLIENT)
endif()