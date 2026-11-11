cmake_minimum_required(VERSION 3.16)
project(auralis-taskmgr C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -O2 -fstack-protector-strong -D_GNU_SOURCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(auralis-taskmgr main.c)

target_link_libraries(auralis-taskmgr PRIVATE
    PkgConfig::GTK4
    m
)

install(TARGETS auralis-taskmgr DESTINATION /usr/local/bin)
install(FILES style.css DESTINATION /usr/share/auralis-taskmgr)
