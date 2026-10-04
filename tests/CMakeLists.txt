find_package(Qt6 6.11 REQUIRED COMPONENTS Test)

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
