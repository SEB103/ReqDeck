find_package(Qt6 6.11 REQUIRED COMPONENTS Test)

# The update controller lives in the application layer; the test compiles it
# directly and takes AppPaths from ReqDeckFramework.
qt_add_executable(tst_updatecontroller
    unit/tst_updatecontroller.cpp
    "${CMAKE_SOURCE_DIR}/app/updatecontroller.cpp"
    "${CMAKE_SOURCE_DIR}/app/updatecontroller.h"
)

target_include_directories(tst_updatecontroller
    PRIVATE
        "${CMAKE_SOURCE_DIR}/app"
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(tst_updatecontroller
    PRIVATE
        ReqDeckFramework
        Qt6::Gui
        Qt6::Network
        Qt6::Test
)

add_test(NAME UpdateController.Unit COMMAND tst_updatecontroller)

qt_add_executable(tst_apppaths
    unit/tst_apppaths.cpp
)

target_include_directories(tst_apppaths
    PRIVATE
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(tst_apppaths
    PRIVATE
        ReqDeckFramework
        Qt6::Test
)

add_test(NAME AppPaths.Unit COMMAND tst_apppaths)

qt_add_executable(tst_logmodel
    unit/tst_logmodel.cpp
)

target_link_libraries(tst_logmodel
    PRIVATE
        ReqDeckFramework
        Qt6::Test
)

add_test(NAME LogModel.Unit COMMAND tst_logmodel)

qt_add_executable(tst_workspaceserializer
    unit/tst_workspaceserializer.cpp
)

target_link_libraries(tst_workspaceserializer
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME WorkspaceSerializer.Unit COMMAND tst_workspaceserializer)

qt_add_executable(tst_workspacemanager
    unit/tst_workspacemanager.cpp
)

target_include_directories(tst_workspacemanager
    PRIVATE
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(tst_workspacemanager
    PRIVATE
        ReqDeckQmlApi
        Qt6::Test
)

add_test(NAME WorkspaceManager.Unit COMMAND tst_workspacemanager)

qt_add_executable(tst_requestspec
    unit/tst_requestspec.cpp
)

target_link_libraries(tst_requestspec
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME RequestSpec.Unit COMMAND tst_requestspec)

qt_add_executable(tst_variableresolver
    unit/tst_variableresolver.cpp
)

target_link_libraries(tst_variableresolver
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME VariableResolver.Unit COMMAND tst_variableresolver)

qt_add_executable(tst_requestbuilder
    unit/tst_requestbuilder.cpp
)

target_link_libraries(tst_requestbuilder
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME RequestBuilder.Unit COMMAND tst_requestbuilder)

# Local HTTP/1.1 server for network tests; tests never contact external hosts.
qt_add_library(ReqDeckTestSupport STATIC
    support/mockhttpserver.cpp
    support/mockhttpserver.h
)

target_include_directories(ReqDeckTestSupport
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/support"
)

target_link_libraries(ReqDeckTestSupport
    PUBLIC
        Qt6::Network
)

qt_add_executable(tst_httpexecutor
    unit/tst_httpexecutor.cpp
)

target_link_libraries(tst_httpexecutor
    PRIVATE
        ReqDeckCore
        ReqDeckTestSupport
        Qt6::Test
)

add_test(NAME HttpExecutor.Unit COMMAND tst_httpexecutor)

qt_add_executable(tst_curlconverter
    unit/tst_curlconverter.cpp
)

target_link_libraries(tst_curlconverter
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME CurlConverter.Unit COMMAND tst_curlconverter)

qt_add_executable(tst_requestjson
    unit/tst_requestjson.cpp
)

target_link_libraries(tst_requestjson
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME RequestJson.Unit COMMAND tst_requestjson)

qt_add_executable(tst_secretstore
    unit/tst_secretstore.cpp
)

target_link_libraries(tst_secretstore
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME SecretStore.Unit COMMAND tst_secretstore)

qt_add_executable(tst_historystore
    unit/tst_historystore.cpp
)

target_link_libraries(tst_historystore
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME HistoryStore.Unit COMMAND tst_historystore)

qt_add_executable(tst_samples
    unit/tst_samples.cpp
)

target_compile_definitions(tst_samples
    PRIVATE REQDECK_SAMPLES_FILE="${CMAKE_SOURCE_DIR}/resources/samples/Samples.reqdeck"
)

target_link_libraries(tst_samples
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME Samples.Unit COMMAND tst_samples)

qt_add_executable(tst_collectiontree
    unit/tst_collectiontree.cpp
)

target_link_libraries(tst_collectiontree
    PRIVATE
        ReqDeckCore
        Qt6::Test
)

add_test(NAME CollectionTree.Unit COMMAND tst_collectiontree)

qt_add_executable(tst_collectiontreemodel
    unit/tst_collectiontreemodel.cpp
)

target_include_directories(tst_collectiontreemodel
    PRIVATE
        "${REQDECK_GENERATED_DIR}"
)

# QAbstractItemModelTester touches Qt Gui metatypes.
target_link_libraries(tst_collectiontreemodel
    PRIVATE
        ReqDeckQmlApi
        Qt6::Gui
        Qt6::Test
)

add_test(NAME CollectionTreeModel.Unit COMMAND tst_collectiontreemodel)

qt_add_executable(tst_keyvaluemodel
    unit/tst_keyvaluemodel.cpp
)

target_link_libraries(tst_keyvaluemodel
    PRIVATE
        ReqDeckQmlApi
        Qt6::Gui
        Qt6::Test
)

add_test(NAME KeyValueModel.Unit COMMAND tst_keyvaluemodel)

qt_add_executable(tst_requesteditor
    unit/tst_requesteditor.cpp
)

target_link_libraries(tst_requesteditor
    PRIVATE
        ReqDeckQmlApi
        ReqDeckTestSupport
        Qt6::Test
)

add_test(NAME RequestEditor.Unit COMMAND tst_requesteditor)

qt_add_executable(tst_bodyformatter
    unit/tst_bodyformatter.cpp
)

target_link_libraries(tst_bodyformatter
    PRIVATE
        ReqDeckFramework
        Qt6::Test
)

add_test(NAME BodyFormatter.Unit COMMAND tst_bodyformatter)

qt_add_executable(tst_responseview
    unit/tst_responseview.cpp
)

target_link_libraries(tst_responseview
    PRIVATE
        ReqDeckQmlApi
        Qt6::Test
)

add_test(NAME ResponseView.Unit COMMAND tst_responseview)

qt_add_executable(tst_bodyhighlighter
    unit/tst_bodyhighlighter.cpp
)

target_link_libraries(tst_bodyhighlighter
    PRIVATE
        ReqDeckQmlApi
        Qt6::Gui
        Qt6::Test
)

add_test(NAME BodyHighlighter.Unit COMMAND tst_bodyhighlighter)

find_package(Qt6 6.11 REQUIRED COMPONENTS QuickTest)

qt_add_executable(tst_qml
    qml/tst_qml.cpp
)

target_sources(tst_qml
    PRIVATE
        qml/tst_menubar.qml
)

target_compile_definitions(tst_qml
    PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)

target_link_libraries(tst_qml
    PRIVATE
        Baseplugin
        ReqDeckFramework
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickTest
)

add_test(NAME ReqDeck.QmlSmoke COMMAND tst_qml)
set_tests_properties(ReqDeck.QmlSmoke PROPERTIES
    ENVIRONMENT "QML2_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)

# The application's screens read its context, so they run against the real
# facades instead of the smoke test's mock.
qt_add_executable(tst_screens
    screens/tst_screens.cpp
)

target_sources(tst_screens
    PRIVATE
        screens/tst_screens.qml
)

target_compile_definitions(tst_screens
    PRIVATE QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/screens"
)

target_include_directories(tst_screens
    PRIVATE
        "${REQDECK_GENERATED_DIR}"
)

target_link_libraries(tst_screens
    PRIVATE
        MainQmlplugin
        Baseplugin
        ReqDeckQmlApi
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickTest
)

qt_import_qml_plugins(tst_screens)

# The icons the screens show, so that a warning in the log means something.
qt_add_resources(tst_screens "screen_icons"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}/resources"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/images/svg/close.svg"
        "${CMAKE_SOURCE_DIR}/resources/images/svg/content_copy.svg"
        "${CMAKE_SOURCE_DIR}/resources/images/svg/folder.svg"
        "${CMAKE_SOURCE_DIR}/resources/images/svg/note_add.svg"
)

add_test(NAME ReqDeck.Screens COMMAND tst_screens)
set_tests_properties(ReqDeck.Screens PROPERTIES
    ENVIRONMENT "QML2_IMPORT_PATH=${QT_QML_OUTPUT_DIRECTORY}"
)
