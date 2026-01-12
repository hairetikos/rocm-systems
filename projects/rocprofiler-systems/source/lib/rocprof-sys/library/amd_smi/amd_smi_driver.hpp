// Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// with the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// * Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimers.
//
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimers in the
// documentation and/or other materials provided with the distribution.
//
// * Neither the names of Advanced Micro Devices, Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this Software without specific prior written permission.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH
// THE SOFTWARE.

#pragma once

#include <cstdint>
#include <memory>

#if ROCPROFSYS_USE_ROCM > 0
#    include <amd_smi/amdsmi.h>
#endif

namespace rocprofsys
{
namespace amd_smi
{

#if ROCPROFSYS_USE_ROCM > 0

struct amd_smi_driver
{
    static amdsmi_status_t init(uint64_t init_flags = AMDSMI_INIT_AMD_GPUS)
    {
        return amdsmi_init(init_flags);
    }

    static amdsmi_status_t shutdown() { return amdsmi_shut_down(); }

    static amdsmi_status_t get_version(amdsmi_version_t* version)
    {
        return amdsmi_get_lib_version(version);
    }

    static amdsmi_status_t get_socket_handles(uint32_t*             socket_count,
                                              amdsmi_socket_handle* socket_handles)
    {
        return amdsmi_get_socket_handles(socket_count, socket_handles);
    }

    static amdsmi_status_t get_processor_handles(
        amdsmi_socket_handle socket_handle, uint32_t* processor_count,
        amdsmi_processor_handle* processor_handles)
    {
        return amdsmi_get_processor_handles(socket_handle, processor_count,
                                            processor_handles);
    }

    static amdsmi_status_t get_processor_type(amdsmi_processor_handle processor_handle,
                                              processor_type_t*       processor_type)
    {
        return amdsmi_get_processor_type(processor_handle, processor_type);
    }

    static amdsmi_status_t get_memory_usage(amdsmi_processor_handle processor_handle,
                                            amdsmi_memory_type_t type, uint64_t* usage)
    {
        return amdsmi_get_gpu_memory_usage(processor_handle, type, usage);
    }

    static amdsmi_status_t get_metrics_info(amdsmi_processor_handle processor_handle,
                                            amdsmi_gpu_metrics_t*   metrics)
    {
        return amdsmi_get_gpu_metrics_info(processor_handle, metrics);
    }
};

struct amd_smi_driver_factory
{
    using driver_t = amd_smi_driver;

    static std::shared_ptr<driver_t> create_driver()
    {
        return std::make_shared<driver_t>();
    }
};

#endif  // ROCPROFSYS_USE_ROCM > 0

}  // namespace amd_smi
}  // namespace rocprofsys
