
if(NOT DEFINED ENV{IDF_PATH})
  message(FATAL_ERROR "The IDF_PATH environment variable must point to the location of the ESP-IDF.")
endif()

message("Building the EI Firmware for esp32")

idf_component_register(
    SRCS src/main.cpp
    INCLUDE_DIRS include
    REQUIRES edge-impulse-ingestion-sdk-c
    )