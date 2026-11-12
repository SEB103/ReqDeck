# Generic application infrastructure with no HTTP knowledge: data-location
# policy, diagnostics vocabulary, and the in-application log models.
qt_add_library(ReqDeckFramework STATIC)

target_sources(ReqDeckFramework
    PRIVATE
        framework/apppaths.cpp
        framework/apppaths.h
        framework/bodyformatter.cpp
        framework/bodyformatter.h
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
        core/history/historystore.cpp
        core/history/historystore.h
        core/http/cookiestore.cpp
        core/http/cookiestore.h
        core/http/curlconverter.cpp
        core/http/curlconverter.h
        core/http/httpencoding.cpp
        core/http/httpencoding.h
        core/http/httpexecutor.cpp
        core/http/httpexecutor.h
        core/http/requestbuilder.cpp
        core/http/requestbuilder.h
        core/http/requestspec.cpp
        core/http/requestspec.h
        core/http/responsedata.h
        core/http/variableresolver.cpp
        core/http/variableresolver.h
        core/import/importresult.h
        core/import/insomniaimporter.cpp
        core/import/insomniaimporter.h
        core/import/openapiimporter.cpp
        core/import/openapiimporter.h
        core/import/yamljson.cpp
        core/import/yamljson.h
        core/import/postmanimporter.cpp
        core/import/postmanimporter.h
        core/workspace/collectiontree.cpp
        core/workspace/inheritance.cpp
        core/workspace/inheritance.h
        core/workspace/collectiontree.h
        core/workspace/jsonfile.cpp
        core/workspace/jsonfile.h
        core/workspace/requestjson.cpp
        core/workspace/requestjson.h
        core/workspace/secretcipher.cpp
        core/workspace/secretcipher.h
        core/workspace/workspacebackup.cpp
        core/workspace/workspacebackup.h
        core/workspace/secretstore.cpp
        core/workspace/secretstore.h
        core/workspace/workspacedata.cpp
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
        Qt6::Network
    PRIVATE
        ReqDeck::yaml-cpp
)

# DPAPI, part of Windows, encrypts the secret values.
if(WIN32)
    target_link_libraries(ReqDeckCore PRIVATE Crypt32)
endif()

# QObject facades exposed to QML as context properties.
qt_add_library(ReqDeckQmlApi STATIC)

target_sources(ReqDeckQmlApi
    PRIVATE
        qmlapi/bodyhighlighter.cpp
        qmlapi/bodyhighlighter.h
        qmlapi/collectiontreemodel.cpp
        qmlapi/cookiemodel.cpp
        qmlapi/cookiemodel.h
        qmlapi/collectiontreemodel.h
        qmlapi/environmentmodel.cpp
        qmlapi/foldereditor.cpp
        qmlapi/importcontroller.cpp
        qmlapi/importcontroller.h
        qmlapi/foldereditor.h
        qmlapi/environmentmodel.h
        qmlapi/environmentvariablesmodel.cpp
        qmlapi/environmentvariablesmodel.h
        qmlapi/helplibrary.cpp
        qmlapi/helplibrary.h
        qmlapi/helplocator.cpp
        qmlapi/helplocator.h
        qmlapi/historymodel.cpp
        qmlapi/historymodel.h
        qmlapi/keyvaluemodel.cpp
        qmlapi/keyvaluemodel.h
        qmlapi/requesteditor.cpp
        qmlapi/requesteditor.h
        qmlapi/responseview.cpp
        qmlapi/responseview.h
        qmlapi/variablehighlighter.cpp
        qmlapi/variablehighlighter.h
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
        Qt6::Gui
        Qt6::Quick
)
