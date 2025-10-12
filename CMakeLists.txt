cmake_minimum_required(VERSION 3.20)
project(ProtoypeA)

set(CLAY_OUT_DIR ${CMAKE_SOURCE_DIR})
file(MAKE_DIRECTORY ${CLAY_OUT_DIR})

add_executable(ProtoypeA main.cpp)
target_include_directories(ProtoypeA PUBLIC clay.h)
set_target_properties(ProtoypeA PROPERTIES
    OUTPUT_NAME index
    SUFFIX ".wasm"
    RUNTIME_OUTPUT_DIRECTORY ${CLAY_OUT_DIR}
)

set(WASM_FLAGS
    -Wall -Os -DCLAY_WASM -Wno-reorder-init-list
)

set(WASM_LINK_FLAGS
    -Wl,--strip-all -Wl,--export-dynamic -Wl,--no-entry
    -Wl,--export=__heap_base -Wl,--export=ACTIVE_RENDERER_INDEX
    -Wl,--initial-memory=6553600
)

target_compile_options(ProtoypeA PRIVATE ${WASM_FLAGS})
target_link_options(ProtoypeA PRIVATE ${WASM_LINK_FLAGS})
