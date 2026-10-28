cmake_minimum_required(VERSION 3.24)
project(lmbetter VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(LMBETTER_FULL_STATIC "Link the complete executable statically" ON)
set(LMBETTER_HOST_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/schema/lmbetter.host.json")

# ---- vendored tree-sitter ---------------------------------------------------
# third_party/tree-sitter and third_party/tree-sitter-json are git-ignored and
# synced by scripts/sync-deps.sh, pinned by deps.lock. The C library is
# compiled through its amalgamation lib/src/lib.c alone (which #includes the
# individual translation units); the JSON grammar contributes src/parser.c
# only (no external scanner).
set(TS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter")
set(TSJSON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-json")

set(TS_SOURCES
  "${TS_ROOT}/lib/src/lib.c"
  "${TSJSON_ROOT}/src/parser.c"
)

# ---- embedded DSL schema and notices ---------------------------------------
set(SCHEMA_JSON "${CMAKE_CURRENT_SOURCE_DIR}/schema/lmbetter.dsl.json")
# Keep the exact host bytes; standard declarations are compiler built-ins.
configure_file("${LMBETTER_HOST_SCHEMA}" "${SCHEMA_JSON}" COPYONLY)
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(GENERATED_SCHEMA "${GENERATED_DIR}/schema_embedded.hpp")
set(GENERATED_NOTICES "${GENERATED_DIR}/notices_embedded.hpp")
add_custom_command(
  OUTPUT "${GENERATED_SCHEMA}"
  COMMAND "${CMAKE_COMMAND}"
    -DINPUT="${SCHEMA_JSON}"
    -DOUTPUT="${GENERATED_SCHEMA}"
    -DNAME=embedded_schema
    -DCOMMENT=schema/lmbetter.dsl.json
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_schema.cmake"
  DEPENDS "${SCHEMA_JSON}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_schema.cmake"
  COMMENT "Embedding schema/lmbetter.dsl.json"
)
add_custom_command(
  OUTPUT "${GENERATED_NOTICES}"
  COMMAND "${CMAKE_COMMAND}"
    -DINPUT="${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    -DOUTPUT="${GENERATED_NOTICES}"
    -DNAME=embedded_notices
    -DCOMMENT=THIRD_PARTY_NOTICES.md
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_schema.cmake"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_schema.cmake"
  COMMENT "Embedding THIRD_PARTY_NOTICES.md"
)
add_custom_target(lmbetter_schema DEPENDS "${GENERATED_SCHEMA}" "${GENERATED_NOTICES}")

add_executable(lmbetter
  cpp/main.cpp
  cpp/host.cpp
  ${TS_SOURCES}
  "${GENERATED_SCHEMA}"
  "${GENERATED_NOTICES}"
)
add_dependencies(lmbetter lmbetter_schema)

target_include_directories(lmbetter PRIVATE
  cpp
  "${TS_ROOT}/lib/include"
  "${TS_ROOT}/lib/src"
  "${GENERATED_DIR}"
)
# Same feature-test defines as the upstream tree-sitter Makefile.
target_compile_definitions(lmbetter PRIVATE _POSIX_C_SOURCE=200112L _DEFAULT_SOURCE)
# tree-sitter sources are third-party: silence their warnings, keep ours strict.
set_source_files_properties(${TS_SOURCES} PROPERTIES
  COMPILE_OPTIONS "-w"
)
target_compile_options(lmbetter PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(lmbetter PRIVATE LMBETTER_VERSION="${PROJECT_VERSION}")

# Static linking: no runtime dependency beyond the OS C library.
# ("Clang" also matches AppleClang; macOS has no -static/-static-libstdc++.)
if((CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang") AND NOT APPLE)
  target_link_options(lmbetter PRIVATE -static-libstdc++ -static-libgcc)
  if(LMBETTER_FULL_STATIC)
    target_link_options(lmbetter PRIVATE -static)
  endif()
endif()
