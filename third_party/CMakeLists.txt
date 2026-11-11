# SPDX-FileCopyrightText: Copyright (C) 2026 ReqDeck Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Third-party libraries built from vendored sources. See the VENDORED.md file of
# each library for its version and origin.

# yaml-cpp: parses YAML files for the import (OpenAPI documents in YAML).
add_library(reqdeck_yaml_cpp STATIC)
add_library(ReqDeck::yaml-cpp ALIAS reqdeck_yaml_cpp)

target_sources(reqdeck_yaml_cpp
    PRIVATE
        yaml-cpp/src/binary.cpp
        yaml-cpp/src/convert.cpp
        yaml-cpp/src/depthguard.cpp
        yaml-cpp/src/directives.cpp
        yaml-cpp/src/emit.cpp
        yaml-cpp/src/emitfromevents.cpp
        yaml-cpp/src/emitter.cpp
        yaml-cpp/src/emitterstate.cpp
        yaml-cpp/src/emitterutils.cpp
        yaml-cpp/src/exceptions.cpp
        yaml-cpp/src/exp.cpp
        yaml-cpp/src/fptostring.cpp
        yaml-cpp/src/memory.cpp
        yaml-cpp/src/node.cpp
        yaml-cpp/src/node_data.cpp
        yaml-cpp/src/nodebuilder.cpp
        yaml-cpp/src/nodeevents.cpp
        yaml-cpp/src/null.cpp
        yaml-cpp/src/ostream_wrapper.cpp
        yaml-cpp/src/parse.cpp
        yaml-cpp/src/parser.cpp
        yaml-cpp/src/regex_yaml.cpp
        yaml-cpp/src/scanner.cpp
        yaml-cpp/src/scanscalar.cpp
        yaml-cpp/src/scantag.cpp
        yaml-cpp/src/scantoken.cpp
        yaml-cpp/src/simplekey.cpp
        yaml-cpp/src/singledocparser.cpp
        yaml-cpp/src/stream.cpp
        yaml-cpp/src/tag.cpp
)

target_include_directories(reqdeck_yaml_cpp
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/include"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/yaml-cpp/src"
)

# Static build: no dllexport/dllimport in the public headers.
target_compile_definitions(reqdeck_yaml_cpp PUBLIC YAML_CPP_STATIC_DEFINE)
target_compile_features(reqdeck_yaml_cpp PUBLIC cxx_std_17)
