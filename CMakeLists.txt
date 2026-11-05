cmake_minimum_required(VERSION 3.21)

project(cvat-native
    VERSION 0.1.0
    DESCRIPTION "Native Qt6 annotation client for a CVAT backend"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(CVATNATIVE_BUILD_TESTS "Build unit tests" ON)
option(CVATNATIVE_ENABLE_ASAN "Enable AddressSanitizer and UBSan" OFF)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Widgets
    Sql
    Concurrent
)

qt_standard_project_setup()

if(CVATNATIVE_ENABLE_ASAN)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

add_library(cvat_core STATIC
    # Q_OBJECT headers must be listed so AUTOMOC generates their meta-objects.
    include/cvatnative/api/ApiClient.h
    include/cvatnative/media/MediaService.h
    include/cvatnative/annotation/AnnotationModel.h

    src/core/Logging.cpp
    src/core/Settings.cpp
    src/core/Credentials.cpp

    src/api/ApiClient.cpp
    src/api/Models.cpp
    src/api/AuthApi.cpp
    src/api/UsersApi.cpp
    src/api/ProjectsApi.cpp
    src/api/OrganizationsApi.cpp
    src/api/TasksApi.cpp
    src/api/JobsApi.cpp
    src/api/AnnotationsApi.cpp
    src/api/LabelsApi.cpp
    src/api/MediaApi.cpp

    src/model/Annotation.cpp

    src/storage/LocalStore.cpp
    src/storage/RecoveryStore.cpp

    src/media/FrameCache.cpp
    src/media/MediaService.cpp

    src/annotation/Geometry.cpp
    src/annotation/AnnotationModel.cpp
)

target_include_directories(cvat_core PUBLIC include)
target_link_libraries(cvat_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
)
target_compile_definitions(cvat_core PUBLIC
    CVATNATIVE_VERSION="${PROJECT_VERSION}"
    QT_NO_CAST_FROM_ASCII
    QT_NO_CAST_TO_ASCII
)
target_compile_options(cvat_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(cvat_ui STATIC
    include/cvatnative/ui/MainWindow.h
    include/cvatnative/ui/AnnotationCanvas.h
    include/cvatnative/ui/LabelPanel.h
    include/cvatnative/ui/SettingsDialog.h
    include/cvatnative/ui/LoginDialog.h
    include/cvatnative/ui/DashboardPage.h
    include/cvatnative/ui/JobWorkspace.h

    src/ui/MainWindow.cpp
    src/ui/LoginDialog.cpp
    src/ui/DashboardPage.cpp
    src/ui/JobWorkspace.cpp
    src/ui/AnnotationCanvas.cpp
    src/ui/AnnotationItems.cpp
    src/ui/LabelPanel.cpp
    src/ui/SettingsDialog.cpp
)
target_include_directories(cvat_ui PUBLIC include)
target_link_libraries(cvat_ui PUBLIC cvat_core Qt6::Widgets)
target_compile_options(cvat_ui PRIVATE -Wall -Wextra -Wpedantic)

add_executable(cvat-native src/app/main.cpp)
target_link_libraries(cvat-native PRIVATE cvat_ui)

install(TARGETS cvat-native RUNTIME DESTINATION bin)
install(FILES packaging/cvat-native.desktop DESTINATION share/applications)

if(CVATNATIVE_BUILD_TESTS)
    enable_testing()
    find_package(GTest REQUIRED)

    add_executable(cvat_core_tests
        tests/test_main.cpp
        tests/test_result.cpp
        tests/test_api_client.cpp
        tests/test_annotation.cpp
        tests/test_geometry.cpp
        tests/test_local_store.cpp
        tests/test_frame_cache.cpp
    )
    target_link_libraries(cvat_core_tests PRIVATE cvat_core GTest::gtest Qt6::Network Qt6::Sql)
    target_compile_options(cvat_core_tests PRIVATE -Wall -Wextra -Wpedantic)
    include(GoogleTest)
    gtest_discover_tests(cvat_core_tests)
endif()
