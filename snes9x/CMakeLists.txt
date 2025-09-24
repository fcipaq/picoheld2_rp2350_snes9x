add_library(snes9x_rp2350 INTERFACE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

target_sources(
    snes9x_rp2350
    INTERFACE ${CMAKE_CURRENT_LIST_DIR}/apu_blargg.c
			  ${CMAKE_CURRENT_LIST_DIR}/clip.c
              ${CMAKE_CURRENT_LIST_DIR}/dma.c
              ${CMAKE_CURRENT_LIST_DIR}/getset.c
              ${CMAKE_CURRENT_LIST_DIR}/obc1.c
              ${CMAKE_CURRENT_LIST_DIR}/spc700.c apu.c
              ${CMAKE_CURRENT_LIST_DIR}/cpu.c
              ${CMAKE_CURRENT_LIST_DIR}/dsp1.c
              ${CMAKE_CURRENT_LIST_DIR}/gfx.c
              ${CMAKE_CURRENT_LIST_DIR}/ppu.c
              ${CMAKE_CURRENT_LIST_DIR}/srtc.c c4.c
              ${CMAKE_CURRENT_LIST_DIR}/cpuexec.c
              ${CMAKE_CURRENT_LIST_DIR}/dsp1emu.c
              ${CMAKE_CURRENT_LIST_DIR}/globals.c
              ${CMAKE_CURRENT_LIST_DIR}/save.c
              ${CMAKE_CURRENT_LIST_DIR}/tile.c 
              ${CMAKE_CURRENT_LIST_DIR}/c4emu.c
              ${CMAKE_CURRENT_LIST_DIR}/cpuops.c
              ${CMAKE_CURRENT_LIST_DIR}/dsp2emu.c
              ${CMAKE_CURRENT_LIST_DIR}/memmap.c
              ${CMAKE_CURRENT_LIST_DIR}/soundux.c
)

target_include_directories(snes9x_rp2350 INTERFACE ${CMAKE_CURRENT_LIST_DIR})

#target_link_libraries(snes9x_rp2350 INTERFACE pico_stdlib hardware_spi hardware_gpio pico_flash
#                                              hardware_exception hardware_sync)

