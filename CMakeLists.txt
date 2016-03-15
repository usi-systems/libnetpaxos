cmake_minimum_required (VERSION 2.8.4)
project (NetPaxos)

include_directories(lib/include)
file(GLOB SOURCE_FILES lib/src/*.c)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})
set (EXTRA_LIBS ${EXTRA_LIBS}  ${LIBEVENT_LIBRARIES})

# print message when running cmake
message(STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})
message(STATUS "Extra libs " ${EXTRA_LIBS})

add_library (Libperf ${SOURCE_FILES})
target_link_libraries (Libperf ${EXTRA_LIBS})


file( GLOB APP_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} app/*.c )
foreach( appfile ${APP_SOURCES} )
    get_filename_component( filename ${appfile} NAME )
    string( REPLACE ".c" "" app ${filename} )
    MESSAGE( ${app} )
    add_executable( ${app} ${appfile} )
    target_link_libraries( ${app} Libperf )
endforeach( appfile ${APP_SOURCES} )