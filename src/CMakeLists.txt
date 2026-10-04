# Generic application infrastructure with no HTTP knowledge: data-location
# policy, diagnostics vocabulary, and the in-application log models.
qt_add_library(ReqDeckFramework STATIC)

target_sources(ReqDeckFramework
    PRIVATE
        framework/apppaths.cpp
        framework/apppaths.h
        framework/diagnosticslevel.h
        framework/logfiltermodel.cpp
        framework/logfiltermodel.h
        framework/logmodel.cpp
        framework/logmodel.h
)

target_include_directories(ReqDeckFramework
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
    PRIVATE
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(ReqDeckFramework
    PUBLIC
        Qt6::Core
)

