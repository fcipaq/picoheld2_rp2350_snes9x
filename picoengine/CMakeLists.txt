# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pimoroni_pga2350 CACHE STRING "Board type")

set(HEADERS ${CMAKE_CURRENT_LIST_DIR}/src/pplib.h)

add_library(picoengine INTERFACE)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

target_include_directories(picoengine INTERFACE ${CMAKE_CURRENT_LIST_DIR}/src
                                                ${CMAKE_CURRENT_LIST_DIR}/src/fonts
                                                ${CMAKE_CURRENT_LIST_DIR}/src/graphics
                                                ${CMAKE_CURRENT_LIST_DIR}/src/hardware
                                                ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd
                                                ${CMAKE_CURRENT_LIST_DIR}/src/hardware/controls
                                                ${CMAKE_CURRENT_LIST_DIR}/src/utils
)

target_link_libraries(picoengine INTERFACE pico_stdlib
                                           hardware_spi
                                           hardware_gpio
										   hardware_adc
                                           hardware_dma
                                           hardware_pwm
                                           hardware_pio
                                           hardware_interp
                                           hardware_clocks
                                           hardware_resets
                                           hardware_exception
                                           hardware_sync
                                           pico_flash
)

# Generate PIO headers
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_lut8.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_lut8.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_mcu8.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_mcu8.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_mcu8_doublescan.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_mcu8_doublescan.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_spi.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_pio_spi.pio)

set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_scanout_3x.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_scanout_3x.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_scanout_2x.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_scanout_2x.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_addr_dec.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_addr_dec.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_addr_dec_8bit.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_addr_dec_8bit.pio)
set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_clut.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_pio_clut.pio)

set_source_files_properties(${CMAKE_CURRENT_LIST_DIR}/src/hardware/sound/snd_pio.pio PROPERTIES GENERATED TRUE)
pico_generate_pio_header(picoengine ${CMAKE_CURRENT_LIST_DIR}/src/hardware/sound/snd_pio.pio)

target_sources(picoengine INTERFACE
	  ${CMAKE_CURRENT_LIST_DIR}/src/picoengine.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/bootloader/bootloader.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/sound/sound.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/backlight/backlight.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_dpi/lcd_dpi_init.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_mcu.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_drv/ili9341_drv.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_drv/ili9488_drv.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/lcd/lcd_mcu/lcd_drv/st7789_drv.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/power/power.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/hardware/controls/controls.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/graphics/gbuffers.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/graphics/primitives.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/graphics/blitter.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/graphics/tilemap.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/graphics/colors.cpp
	  ${CMAKE_CURRENT_LIST_DIR}/src/fonts/fonts.cpp
#	  ${CMAKE_CURRENT_LIST_DIR}/src/utils/console.cpppp
)

