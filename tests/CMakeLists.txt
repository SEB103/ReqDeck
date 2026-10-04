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
