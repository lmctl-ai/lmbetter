# Embeds a text file as a constexpr byte array.
# Called with -DINPUT=<file> -DOUTPUT=<hpp> -DNAME=<symbol> [-DCOMMENT=<text>].
file(READ "${INPUT}" _embed_hex HEX)
string(LENGTH "${_embed_hex}" _hex_len)
math(EXPR _byte_len "${_hex_len} / 2")
string(REGEX REPLACE "(..)" "0x\\1," _embed_bytes "${_embed_hex}")
string(REGEX REPLACE ",$" "" _embed_bytes "${_embed_bytes}")
file(WRITE "${OUTPUT}"
"// Generated from ${COMMENT} by cmake/embed_schema.cmake. Do not edit.
#pragma once
namespace lmok {
inline constexpr unsigned char ${NAME}[] = {${_embed_bytes}};
inline constexpr unsigned int ${NAME}_size = ${_byte_len};
}
")
