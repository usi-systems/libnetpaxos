cmake_minimum_required (VERSION 2.8.4)
project (NetPaxos)

include_directories(lib/include)
include_directories(app/include)
file(GLOB SOURCE_FILES lib/src/*.c)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -Wall -Wextra -Wno-unused-parameter -ggdb -g")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules/")
find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})
set (EXTRA_LIBS ${EXTRA_LIBS}  ${LIBEVENT_LIBRARIES})

# print message when running cmake
message(STATUS "This is CMAKE_CXX_FLAGS " ${CMAKE_CXX_FLAGS})

add_library (Libperf ${SOURCE_FILES})
target_link_libraries (Libperf ${EXTRA_LIBS})

# leveldb
include_directories(/usr/local/include/leveldb/include)
link_directories(/usr/local/lib)

set (EXTRA_LIBS ${EXTRA_LIBS} Libperf leveldb m)
message(STATUS "Extra libs " ${EXTRA_LIBS})

file( GLOB APP_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} app/*.c )

foreach( appfile ${APP_SOURCES} )
    get_filename_component( filename ${appfile} NAME )
    string( REPLACE ".c" "" app ${filename} )
    MESSAGE( ${app} )
    add_executable( ${app} ${appfile} )
    target_link_libraries( ${app} ${EXTRA_LIBS})
    SET_TARGET_PROPERTIES( ${app} PROPERTIES COMPILE_FLAGS ${CMAKE_CXX_FLAGS})
endforeach( appfile ${APP_SOURCES} )