cmake_minimum_required(VERSION 3.20.0)

# Find Zephyr. This also loads Zephyr's build system.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(engine-analyser)

# Add FPU flags
target_compile_options(app PRIVATE
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

# Add library path and file
target_include_directories(app PRIVATE lib/nanoedge_ai)
target_link_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/lib/nanoedge_ai)
target_link_libraries(app PRIVATE ${CMAKE_SOURCE_DIR}/lib/nanoedge_ai/libneai.a)

# Add include directories
target_include_directories(app PRIVATE 
    inc
    ${CMAKE_SOURCE_DIR}/lib/nanoedge_ai
)

# Add source files
target_sources(app PRIVATE 
    src/main.c
    src/accelerometer.c
)

# Add NanoEdgeAI memory mapping patch
zephyr_linker_sources(SECTIONS neai_overlay.ld)