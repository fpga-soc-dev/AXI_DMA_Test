cmake_minimum_required(VERSION 3.20)

project(axi-dma-stress-test
	VERSION 1.0.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)

set(SOURCES
    Utilities/FileHandle.cpp
    Utilities/FileMapping.cpp
    Utilities/EPoll.cpp
    DmaBuffer/UDmaBuffer.cpp
    DmaBuffer/UDmaBufferAllocate.cpp
    AxiDataGen/AxiDataGen.cpp
    AxiDma/AxiDma.cpp
    AxiDma/AxiDmaBd.cpp
    AxiDma/AxiDmaBdRing.cpp
    AxiDma/AxiDmaChannel.cpp
    AxiTrafficGen/AxiTrafficGen.cpp
    UIO/Uio.cpp
    main.cpp
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        Utilities/
        DmaBuffer/
        AxiDataGen/
        AxiTrafficGen/
        AxiDma/
        UIO/
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE 
    fmt::fmt
    spdlog::spdlog_header_only
)

add_definitions(-DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_DEBUG)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)


