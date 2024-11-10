cmake_minimum_required(VERSION 3.10)

project(WEB_SOCKET_SERVER_CPP)

add_executable(WEB_SOCKET_SERVER_CPP main.cpp ws.cpp mem.cpp Base64.cpp)

set_property(TARGET WEB_SOCKET_SERVER_CPP PROPERTY RUNTIME_OUTPUT_DIRECTORY build)

find_package(OpenSSL REQUIRED)
target_link_libraries(WEB_SOCKET_SERVER_CPP OpenSSL::SSL)
