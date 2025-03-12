cmake_minimum_required(VERSION 3.10)
project(EvolutionSim VERSION 1.0)

# Set C standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Add compiler optimization flags for Release mode
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")

# Find raylib package
find_package(raylib QUIET)

# If raylib is not found, download and build it
if (NOT raylib_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 4.5.0
    )
    FetchContent_MakeAvailable(raylib)
endif()

# Add executable
add_executable(evolution_sim evolution_sim.c)

# Copy assets directory to build directory
file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# Also copy assets to the output directory
add_custom_command(
    TARGET evolution_sim POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets ${CMAKE_SOURCE_DIR}/assets
)

# Link with raylib
target_link_libraries(evolution_sim PRIVATE raylib m pthread dl)

# Add platform-specific definitions
if (APPLE)
    target_link_libraries(evolution_sim PRIVATE "-framework CoreVideo -framework IOKit -framework Cocoa -framework GLUT -framework OpenGL")
endif()

# Installation rules
install(TARGETS evolution_sim DESTINATION bin)

# Output binary to the 'output' directory
set_target_properties(evolution_sim
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

message(STATUS "Configuring Evolution Simulator")