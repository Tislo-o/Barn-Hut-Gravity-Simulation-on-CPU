cmake_minimum_required(VERSION 3.10)

project(Gravity-Barn-Hut)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3.0 COMPONENTS Graphics Window System REQUIRED)
find_package(OpenMP REQUIRED)

add_executable(Gravity-Barn-Hut main.cpp)
target_compile_options(Gravity-Barn-Hut PRIVATE -O3 -march=native -ffast-math -fopt-info-vec-optimized -fopt-info-vec-missed)
target_link_libraries(Gravity-Barn-Hut SFML::Graphics SFML::Window SFML::System OpenMP::OpenMP_CXX)

add_custom_command(TARGET Gravity-Barn-Hut POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Gravity-Barn-Hut>/assets
)