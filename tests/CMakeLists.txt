set(TESTABLE_PROJECT_SOURCES_ABS)
foreach(source_file IN LISTS TESTABLE_PROJECT_SOURCES)
    list(APPEND TESTABLE_PROJECT_SOURCES_ABS "${CMAKE_SOURCE_DIR}/${source_file}")
endforeach()

set(PROJECT_HEADERS_ABS)
foreach(header_file IN LISTS PROJECT_HEADERS)
    list(APPEND PROJECT_HEADERS_ABS "${CMAKE_SOURCE_DIR}/${header_file}")
endforeach()

if(FLING_BUILD_TESTS)
    set(TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/fixtures/test_support.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/backend_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/download_manager_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/file_system_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/game_mapping_manager_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/app_update_manager_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/unit/translation_database_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/search_manager_integration_test.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/integration/database_update_manager_integration_test.cpp"
    )

    add_executable(${PROJECT_NAME}Tests
        ${TESTABLE_PROJECT_SOURCES_ABS}
        ${PROJECT_HEADERS_ABS}
        ${TEST_SOURCES}
    )

    set_target_properties(${PROJECT_NAME}Tests PROPERTIES
        OUTPUT_NAME "FLiNG Downloader Tests"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    target_include_directories(${PROJECT_NAME}Tests PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/tests
        ${CMAKE_SOURCE_DIR}/tests/fixtures
    )

    target_link_libraries(${PROJECT_NAME}Tests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        Qt6::Concurrent
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        CURL::libcurl
        nlohmann_json::nlohmann_json
        SQLiteCpp
        pugixml::pugixml
        ${OpenCV_LIBS}
        GTest::gtest
    )

    target_compile_definitions(${PROJECT_NAME}Tests PRIVATE
        CURL_STATICLIB
        FLING_APP_VERSION="${APP_VERSION}"
    )

    if(MSVC)
        set_property(TARGET ${PROJECT_NAME}Tests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
        target_link_libraries(${PROJECT_NAME}Tests PRIVATE
            ws2_32.lib
            wldap32.lib
            crypt32.lib
            normaliz.lib
            User32.lib
            Gdi32.lib
            Advapi32.lib
        )
    endif()

    add_test(NAME ${PROJECT_NAME}Tests COMMAND $<TARGET_FILE:${PROJECT_NAME}Tests>)
endif()

if(FLING_BUILD_BENCHMARKS)
    set(BENCHMARK_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/performance/benchmark_main.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/performance/cover_extractor_benchmark.cpp"
        "${CMAKE_SOURCE_DIR}/src/CoverExtractor.cpp"
    )

    add_executable(FLiNGDownloaderBenchmarks
        ${BENCHMARK_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/include/CoverExtractor.h
    )

    set_target_properties(FLiNGDownloaderBenchmarks PROPERTIES
        OUTPUT_NAME "FLiNG Downloader Benchmarks"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )

    target_include_directories(FLiNGDownloaderBenchmarks PRIVATE
        ${CMAKE_SOURCE_DIR}/src/include
        ${CMAKE_SOURCE_DIR}/tests
    )

    target_link_libraries(FLiNGDownloaderBenchmarks PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Network
        benchmark::benchmark
        ${OpenCV_LIBS}
    )

    target_compile_definitions(FLiNGDownloaderBenchmarks PRIVATE
        FLING_BENCHMARK_SAMPLE_DIR="${CMAKE_SOURCE_DIR}/tests/resources/fling_trainer_screenshot"
    )

    if(MSVC)
        set_property(TARGET FLiNGDownloaderBenchmarks PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
        target_compile_options(FLiNGDownloaderBenchmarks PRIVATE /utf-8)
        target_link_libraries(FLiNGDownloaderBenchmarks PRIVATE
            User32.lib
            Gdi32.lib
        )
    endif()
endif()
