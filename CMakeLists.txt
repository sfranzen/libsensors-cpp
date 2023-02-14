cmake_minimum_required(VERSION 3.1.0)

project(sensors-c++ VERSION 3.6.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

find_library(libsensors sensors)
if(libsensors STREQUAL "libsensors-NOTFOUND")
    message(FATAL_ERROR "libsensors not found!")
endif()

list(APPEND SRC
    src/sensors.cpp
    src/error.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC})

set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

target_link_libraries(${PROJECT_NAME} PRIVATE sensors)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE src
)

include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT "${PROJECT_NAME}-config"
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT "${PROJECT_NAME}-config"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)
export(TARGETS ${PROJECT_NAME}
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake"
)

add_subdirectory(test)
