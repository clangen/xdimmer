#cmake -DCMAKE_BUILD_TYPE=Release .
#cmake -DCMAKE_BUILD_TYPE=Debug .

cmake_minimum_required(VERSION 3.1)

project(xdimmer)

set (xdimmer_VERSION_MAJOR 0)
set (xdimmer_VERSION_MINOR 1)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

if (${LINK_STATICALLY} MATCHES "true")
  set(Boost_USE_STATIC_LIBS ON)
endif()

set (EXECUTABLE_OUTPUT_PATH ${xdimmer_SOURCE_DIR}/__output)

include_directories (
  "${xdimmer_SOURCE_DIR}/src"
  "${xdimmer_SOURCE_DIR}/src/cursespp/f8n/src"
  "${xdimmer_SOURCE_DIR}/src/cursespp/f8n/src/3rdparty/include"
  "${xdimmer_SOURCE_DIR}/src/cursespp/src"
)

# "/usr/local" doesn't seem to be included by default on macOS 10.12+
if (CMAKE_SYSTEM_NAME MATCHES "Darwin")
  link_directories ("/usr/local/lib")
  include_directories("/usr/local/include")
  include_directories("/usr/local/opt/openssl/include")
endif (CMAKE_SYSTEM_NAME MATCHES "Darwin")

if (EXISTS "/etc/arch-release" OR EXISTS "/etc/manjaro-release" OR NO_NCURSESW)
  add_definitions (-DNO_NCURSESW)
elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
  add_definitions (-DNO_NCURSESW)
endif()

set (xdimmer_SRCS
  ./src/app/main.cpp
)

add_executable(xdimmer ${xdimmer_SRCS})

add_subdirectory("${xdimmer_SOURCE_DIR}/src/cursespp/")

add_dependencies(xdimmer f8n cursespp)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(xdimmer ncursesw panelw f8n cursespp)
else (CMAKE_SYSTEM_NAME MATCHES "Linux")
  target_link_libraries(xdimmer curses panel)
endif (CMAKE_SYSTEM_NAME MATCHES "Linux")

# install(
#   FILES lib/libxdimmer.a
#   DESTINATION lib/
# )
