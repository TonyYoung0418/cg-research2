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

find_package(OpenGL)
find_package(GLUT)

if(OPENGL_FOUND AND GLUT_FOUND)
    add_executable(MirrorReflectionViewer
        src/opengl_viewer.cpp
        src/scene_builder.cpp
    )
    target_include_directories(MirrorReflectionViewer PRIVATE ${OPENGL_INCLUDE_DIRS} ${GLUT_INCLUDE_DIRS})
    target_link_libraries(MirrorReflectionViewer PRIVATE ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
    target_compile_definitions(MirrorReflectionViewer PRIVATE GL_SILENCE_DEPRECATION)
    target_compile_options(MirrorReflectionViewer PRIVATE -O2 -Wall -Wextra -pedantic)
else()
    message(WARNING "OpenGL/GLUT not found; MirrorReflectionViewer will not be built.")
endif()
