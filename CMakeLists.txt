cmake_minimum_required(VERSION 3.10)
project(MirrorReflectionPathTracer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(MirrorReflectionRenderer
    src/main.cpp
    src/scene_builder.cpp
)
target_compile_options(MirrorReflectionRenderer PRIVATE -O3 -Wall -Wextra -pedantic)
