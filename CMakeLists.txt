cmake_minimum_required(VERSION 3.7)
project(dhcp_1)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES
        main.c
       # test.c test.h
        dhcp.c dhcp.h

        )
add_executable(dhcp_1 ${SOURCE_FILES})