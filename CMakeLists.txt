cmake_minimum_required(VERSION 3.10)

project(WEB_SOCKET_SERVER_C)

add_executable(WEB_SOCKET_SERVER_C main.c ws.c mem.c Base64.c)

set(CMAKE_C_FLAGS "-fsanitize=address")

set_property(TARGET WEB_SOCKET_SERVER_C PROPERTY RUNTIME_OUTPUT_DIRECTORY build)

find_package(OpenSSL REQUIRED)
target_link_libraries(WEB_SOCKET_SERVER_C OpenSSL::SSL m)
