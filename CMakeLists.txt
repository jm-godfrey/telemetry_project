cmake_minimum_required(VERSION 3.15)
project(telemetry_project VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(include)

# Add source files here. Update as needed.
set(SOURCES
    src/main.cpp
    src/gps.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

# Optionally add include directories or libraries below.
# target_include_directories(${PROJECT_NAME} PRIVATE include)
# target_link_libraries(${PROJECT_NAME} PRIVATE some_library)
