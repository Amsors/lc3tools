# find directories with includes
include_directories(../backend)
include_directories(../common)
find_package(Threads REQUIRED)

# generate package
file(GLOB FRAMEWORK_SOURCES *.cpp *.h)
add_library(framework OBJECT ${FRAMEWORK_SOURCES})

# get all necessary files
if(BUILD_SAMPLES)
    file(GLOB TEST_SOURCES tests/*.cpp tests/samples/*.cpp)
else()
    file(GLOB TEST_SOURCES tests/*.cpp)
endif()

foreach(TEST_SOURCE ${TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    if(TEST_NAME MATCHES "_test$")
        add_executable(${TEST_NAME} ${TEST_SOURCE})
    else()
        add_executable(${TEST_NAME} ${TEST_SOURCE} $<TARGET_OBJECTS:common> $<TARGET_OBJECTS:framework>)
    endif()
    target_include_directories(${TEST_NAME} PUBLIC .)
    target_link_libraries(${TEST_NAME} lc3core)
    if(TEST_NAME MATCHES "_test$")
        target_link_libraries(${TEST_NAME} Threads::Threads)
        add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
        set_tests_properties(${TEST_NAME} PROPERTIES TIMEOUT 5)
    endif()
endforeach()
