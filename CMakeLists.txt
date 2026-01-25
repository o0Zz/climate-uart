file(GLOB_RECURSE srcs "src/*.cpp")

if(ESP_PLATFORM)
    idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "include" "src"
        REQUIRES driver esp_timer log
    )
else()
    add_library(climate_uart ${srcs})
    target_include_directories(climate_uart PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include" "${CMAKE_CURRENT_LIST_DIR}/src")
endif()
