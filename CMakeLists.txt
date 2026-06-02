cmake_minimum_required(VERSION 3.16)
project(plsql_unwrap CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(unwrap
  src/main.cpp
  src/embedded.cpp
  src/unwrap_v2.cpp
  src/wrap_v2.cpp
  src/obfuscate.cpp
  src/unwrap_v1.cpp
  src/unwrap_v1_state.cpp
  src/unwrap_v1_emit.cpp
  src/unwrap_v1_parse.cpp
  src/unwrap_v1_nodes.cpp
  src/diana_grammar.cpp
)

target_include_directories(unwrap PRIVATE src)
target_link_libraries(unwrap PRIVATE ZLIB::ZLIB OpenSSL::Crypto)
