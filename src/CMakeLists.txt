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
        framework/retranslatablestatus.h
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

# HTTP domain and persistence without any QML dependency.
qt_add_library(ReqDeckCore STATIC)

target_sources(ReqDeckCore
    PRIVATE
        core/workspace/workspacedata.h
        core/workspace/workspaceserializer.cpp
        core/workspace/workspaceserializer.h
)

target_include_directories(ReqDeckCore
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ReqDeckCore
    PUBLIC
        Qt6::Core
)

# QObject facades exposed to QML as context properties.
qt_add_library(ReqDeckQmlApi STATIC)

target_sources(ReqDeckQmlApi
    PRIVATE
        qmlapi/workspacemanager.cpp
        qmlapi/workspacemanager.h
)

target_include_directories(ReqDeckQmlApi
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(ReqDeckQmlApi
    PUBLIC
        ReqDeckCore
        ReqDeckFramework
        Qt6::Core
)
