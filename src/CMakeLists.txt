cmake_minimum_required(VERSION 3.5.1)
project(demo CXX)

set(CMAKE_CXX_STANDARD 17)
option(BUILD_STATIC "Set to ON to include static versions of the library" ON)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Output dirs
get_filename_component(PARENT_DIR ${CMAKE_BINARY_DIR} DIRECTORY)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PARENT_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PARENT_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PARENT_DIR}/bin")
# ======== OpenFHE ========
find_package(OpenFHE CONFIG REQUIRED)
if (OpenFHE_FOUND)
    message(STATUS "FOUND PACKAGE OpenFHE")
    message(STATUS "OpenFHE Version: ${BASE_OPENFHE_VERSION}")
    message(STATUS "OpenFHE installed as shared libraries: ${OpenFHE_SHARED}")
    message(STATUS "OpenFHE include files location: ${OpenFHE_INCLUDE}")
    message(STATUS "OpenFHE lib files location: ${OpenFHE_LIBDIR}")
    message(STATUS "OpenFHE Native Backend size: ${OpenFHE_NATIVE_SIZE}")
else()
    message(FATAL_ERROR "PACKAGE OpenFHE NOT FOUND")
endif()

include_directories( ${OPENMP_INCLUDES} )
include_directories( ${OpenFHE_INCLUDE} )
include_directories( ${OpenFHE_INCLUDE}/third-party/include )
include_directories( ${OpenFHE_INCLUDE}/core )
include_directories( ${OpenFHE_INCLUDE}/pke )
include_directories( ${OpenFHE_INCLUDE}/binfhe )



set(C_COMPILE_FLAGS "-Wall -g -O0 ${NATIVE_OPT} -DOPENFHE_VERSION=${OPENFHE_VERSION}")
set(CXX_COMPILE_FLAGS "-Wall -g -O0 ${NATIVE_OPT} -DOPENFHE_VERSION=${OPENFHE_VERSION} ${IGNORE_WARNINGS}")

# ======== Paths & Includes ========
include_directories(${OpenFHE_INCLUDE})
link_directories(${OpenFHE_LIBDIR})
link_directories( ${OPENMP_LIBRARIES} )
if(BUILD_STATIC)
    set( CMAKE_EXE_LINKER_FLAGS "${OpenFHE_EXE_LINKER_FLAGS} -static")
    link_libraries( ${OpenFHE_STATIC_LIBRARIES} )
else()
    set( CMAKE_EXE_LINKER_FLAGS ${OpenFHE_EXE_LINKER_FLAGS} )
    link_libraries( ${OpenFHE_SHARED_LIBRARIES} )
endif()

add_library(mainlib_common utils.cpp)
target_include_directories(mainlib_common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test test.cpp)

set_target_properties(test PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/../openfhe-static/build/lib"
    BUILD_RPATH_USE_ORIGIN TRUE
)
target_link_libraries(test PRIVATE mainlib_common ${OpenFHE_SHARED_LIBRARIES})


