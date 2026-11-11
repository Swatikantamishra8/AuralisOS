cmake_minimum_required(VERSION 3.16)
project(auralis-shell C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Optimization flags for musl / ultra-low memory
add_compile_options(-Wall -Wextra -O2 -fstack-protector-strong -D_GNU_SOURCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(LAYER_SHELL REQUIRED IMPORTED_TARGET gtk4-layer-shell-0)

add_executable(auralis-shell main.c)

target_link_libraries(auralis-shell PRIVATE
    PkgConfig::GTK4
    PkgConfig::LAYER_SHELL
    m
)

install(TARGETS auralis-shell DESTINATION /usr/local/bin)
install(FILES style.css DESTINATION /usr/share/auralis-shell)
