set(amd_smi_sources
    ${CMAKE_CURRENT_LIST_DIR}/amd_smi_driver.cpp
    ${CMAKE_CURRENT_LIST_DIR}/processor.cpp
    ${CMAKE_CURRENT_LIST_DIR}/service.cpp
    ${CMAKE_CURRENT_LIST_DIR}/settings.cpp
    ${CMAKE_CURRENT_LIST_DIR}/perfetto_policy.cpp
    ${CMAKE_CURRENT_LIST_DIR}/rocpd_policy.cpp
    ${CMAKE_CURRENT_LIST_DIR}/amd_smi_impl.cpp
)

add_library(rocprofiler-systems-amd-smi-library OBJECT ${amd_smi_sources})

target_link_libraries(
    rocprofiler-systems-amd-smi-library PRIVATE rocprofiler-systems-googletest-library rocprofiler-systems-core-library
)

if(ROCPROFSYS_BUILD_TESTING)
    add_subdirectory(tests)
endif()
