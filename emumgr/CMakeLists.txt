add_library(emumgr INTERFACE)

target_sources(emumgr INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/emumgr.cpp
    ${CMAKE_CURRENT_LIST_DIR}/emumgr_hal.cpp
    ${CMAKE_CURRENT_LIST_DIR}/heapstrings.cpp
)

target_include_directories(emumgr INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_libraries(emumgr INTERFACE
    pico_stdlib
    hardware_clocks
    hardware_spi
    sparkfun_pico
)

