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

#include "library/amd_smi/common.hpp"
#include "library/amd_smi/processor.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if ROCPROFSYS_USE_ROCM > 0
#    include <amd_smi/amdsmi.h>
#endif

namespace rocprofsys
{
namespace amd_smi
{

#if ROCPROFSYS_USE_ROCM > 0

template <typename DriverFactory>
class service
{
public:
    using driver_t           = typename DriverFactory::driver_t;
    using processor_t        = processor<driver_t>;
    using processor_ptr_t    = std::shared_ptr<processor_t>;
    using processor_vector_t = std::vector<processor_ptr_t>;
    using filter_func_t      = std::function<processor_vector_t(processor_vector_t&)>;

    service()
    : m_driver_api(DriverFactory::create_driver())
    {
        check_status(m_driver_api->init(), "Failed to initialize AMD SMI driver!");

        amdsmi_version_t ver;
        check_status(m_driver_api->get_version(&ver),
                     "Failed to get AMD SMI driver version!");

        m_version = { .numeric_representation = { .major   = ver.major,
                                                  .minor   = ver.minor,
                                                  .release = ver.release },
                      .string_representation  = std::string{ ver.build } };
    }

    ~service() = default;

    service(const service&)            = delete;
    service& operator=(const service&) = delete;
    service(service&&)                 = default;
    service& operator=(service&&)      = default;

    [[nodiscard]] const version& get_version() const noexcept { return m_version; }

    [[nodiscard]] std::shared_ptr<driver_t> get_driver() const noexcept
    {
        return m_driver_api;
    }

    [[nodiscard]] processor_vector_t get_processors(const filter_func_t& filter = nullptr)
    {
        processor_vector_t processors;

        auto   socket_handles = get_socket_handles();
        size_t index          = 0;

        for(auto& socket_handle : socket_handles)
        {
            auto processor_handles = get_processor_handles(socket_handle);

            for(auto& processor_handle : processor_handles)
            {
                processor_type_t processor_type;
                check_status(
                    m_driver_api->get_processor_type(processor_handle, &processor_type),
                    "Failed to get processor type!");

                processors.emplace_back(std::make_shared<processor_t>(
                    m_driver_api, processor_handle, processor_type, index++));
            }
        }

        return (filter != nullptr) ? filter(processors) : processors;
    }

    void shutdown() { m_driver_api->shutdown(); }

private:
    std::vector<amdsmi_socket_handle> get_socket_handles()
    {
        uint32_t count = 0;
        check_status(m_driver_api->get_socket_handles(&count, nullptr),
                     "Failed to get socket count!");

        std::vector<amdsmi_socket_handle> handles(count);
        if(count > 0)
        {
            check_status(m_driver_api->get_socket_handles(&count, handles.data()),
                         "Failed to get socket handles!");
        }

        return handles;
    }

    std::vector<amdsmi_processor_handle> get_processor_handles(
        amdsmi_socket_handle socket_handle)
    {
        uint32_t count = 0;
        check_status(m_driver_api->get_processor_handles(socket_handle, &count, nullptr),
                     "Failed to get processor count!");

        std::vector<amdsmi_processor_handle> handles(count);
        if(count > 0)
        {
            check_status(m_driver_api->get_processor_handles(socket_handle, &count,
                                                             handles.data()),
                         "Failed to get processor handles!");
        }

        return handles;
    }

    std::shared_ptr<driver_t> m_driver_api;
    version                   m_version{};
};

template <typename DriverFactory>
struct smi_service_factory
{
    using smi_service        = service<DriverFactory>;
    using processor_t        = typename smi_service::processor_t;
    using processor_vector_t = typename smi_service::processor_vector_t;

    static std::shared_ptr<smi_service> create_smi_service()
    {
        return std::make_shared<smi_service>();
    }
};

#endif  // ROCPROFSYS_USE_ROCM > 0

}  // namespace amd_smi
}  // namespace rocprofsys
