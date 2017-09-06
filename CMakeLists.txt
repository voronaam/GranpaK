cmake_minimum_required(VERSION 2.6)
project(granpak)

add_executable(granpak main.cpp)

install(TARGETS granpak RUNTIME DESTINATION bin)
