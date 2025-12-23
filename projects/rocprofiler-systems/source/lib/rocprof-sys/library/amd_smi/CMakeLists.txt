set(amd_smi_sources
    ${CMAKE_CURRENT_LIST_DIR}/amd_smi_driver.hpp
    ${CMAKE_CURRENT_LIST_DIR}/processor.hpp
    ${CMAKE_CURRENT_LIST_DIR}/service.hpp
    ${CMAKE_CURRENT_LIST_DIR}/settings.hpp
    ${CMAKE_CURRENT_LIST_DIR}/perfetto_policy.hpp
    ${CMAKE_CURRENT_LIST_DIR}/rocpd_policy.hpp
    ${CMAKE_CURRENT_LIST_DIR}/amd_smi_impl.hpp
)

add_library(rocprofiler-systems-amd-smi-library OBJECT ${amd_smi_sources})

target_link_libraries(
    rocprofiler-systems-amd-smi-library PRIVATE rocprofiler-systems-googletest-library rocprofiler-systems-core-library
)

# if(ROCPROFSYS_BUILD_TESTING)
#     add_subdirectory(tests)
# endif()
