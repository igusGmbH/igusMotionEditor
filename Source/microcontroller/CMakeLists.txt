
cmake_minimum_required(VERSION 2.8)

project(igus_microcontroller)

add_definitions(-DF_CPU=16000000L)

set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)

set(CMAKE_CXX_FLAGS "-g -Wall -O2 -fno-exceptions -mmcu=atmega2560")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS)

set(tname igus_microcontroller)

add_executable(${tname}
	main.cpp
	uart.cpp
	combuf.cpp
	nanotec.cpp
	mem.cpp
	commands.cpp
	motion.cpp
	io.cpp
)

add_custom_command(
	OUTPUT ${tname}.hex
	DEPENDS ${tname}
	COMMAND avr-objcopy -R .eeprom -O ihex ${tname} ${tname}.hex
)

add_custom_target(${tname}_writeflash
	DEPENDS ${tname}.hex genreset
	COMMAND ${CMAKE_HOME_DIRECTORY}/writeflash.sh /dev/ttyUSB0 ${tname}.hex
)

add_custom_command(
	OUTPUT genreset
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/protocol.h ${CMAKE_CURRENT_SOURCE_DIR}/genreset.cpp
	COMMAND g++ -o ${CMAKE_CURRENT_BINARY_DIR}/genreset ${CMAKE_CURRENT_SOURCE_DIR}/genreset.cpp
)
