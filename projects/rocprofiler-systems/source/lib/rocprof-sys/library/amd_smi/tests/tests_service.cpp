// Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// MIT License - See LICENSE file for details.

#include "library/amd_smi/service.hpp"
#include "mock_driver.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstring>
#include <memory>

#if ROCPROFSYS_USE_ROCM > 0

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::SetArgPointee;

namespace rocprofsys
{
namespace amd_smi
{
namespace testing
{

class service_test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_mock_driver = std::make_shared<::testing::NiceMock<mock_driver>>();
        m_mock_driver->set_up_defaults();
        mock_driver_factory::set_mock_driver(m_mock_driver);

        set_up_default_version();
        set_up_default_sockets_and_processors();
    }

    void TearDown() override
    {
        mock_driver_factory::set_mock_driver(nullptr);
        m_mock_driver.reset();
    }

    void set_up_default_version()
    {
        amdsmi_version_t version{};
        version.major   = 1;
        version.minor   = 2;
        version.release = 3;
        version.build   = "test-build";

        ON_CALL(*m_mock_driver, get_version(_))
            .WillByDefault(
                DoAll(SetArgPointee<0>(version), Return(AMDSMI_STATUS_SUCCESS)));
    }

    void set_up_default_sockets_and_processors()
    {
        // Default: 1 socket, 0 processors
        ON_CALL(*m_mock_driver, get_socket_handles(_, _))
            .WillByDefault([](uint32_t* count, amdsmi_socket_handle* handles) {
                *count = 1;
                if(handles != nullptr)
                {
                    handles[0] = reinterpret_cast<amdsmi_socket_handle>(0x100);
                }
                return AMDSMI_STATUS_SUCCESS;
            });

        ON_CALL(*m_mock_driver, get_processor_handles(_, _, _))
            .WillByDefault(
                [](amdsmi_socket_handle, uint32_t* count, amdsmi_processor_handle*) {
                    *count = 0;
                    return AMDSMI_STATUS_SUCCESS;
                });
    }

    void set_up_processors(size_t count)
    {
        ON_CALL(*m_mock_driver, get_processor_handles(_, _, _))
            .WillByDefault([count](amdsmi_socket_handle, uint32_t* proc_count,
                                   amdsmi_processor_handle* handles) {
                *proc_count = static_cast<uint32_t>(count);
                if(handles != nullptr)
                {
                    for(size_t i = 0; i < count; ++i)
                    {
                        handles[i] =
                            reinterpret_cast<amdsmi_processor_handle>(0x1000 + i);
                    }
                }
                return AMDSMI_STATUS_SUCCESS;
            });

        ON_CALL(*m_mock_driver, get_processor_type(_, _))
            .WillByDefault(DoAll(SetArgPointee<1>(AMDSMI_PROCESSOR_TYPE_AMD_GPU),
                                 Return(AMDSMI_STATUS_SUCCESS)));
    }

    std::shared_ptr<::testing::NiceMock<mock_driver>> m_mock_driver;
};

TEST_F(service_test, constructor_initializes_driver_and_version)
{
    service<mock_driver_factory> svc;

    auto version = svc.get_version();
    EXPECT_EQ(version.numeric_representation.major, 1u);
    EXPECT_EQ(version.numeric_representation.minor, 2u);
    EXPECT_EQ(version.numeric_representation.release, 3u);
    EXPECT_EQ(version.string_representation, "test-build");
}

TEST_F(service_test, get_driver_returns_shared_driver)
{
    service<mock_driver_factory> svc;

    auto driver = svc.get_driver();
    EXPECT_NE(driver, nullptr);
    EXPECT_EQ(driver.get(), m_mock_driver.get());
}

TEST_F(service_test, get_processors_returns_empty_when_no_processors)
{
    service<mock_driver_factory> svc;

    auto processors = svc.get_processors();
    EXPECT_TRUE(processors.empty());
}

TEST_F(service_test, get_processors_returns_all_processors)
{
    set_up_processors(3);

    service<mock_driver_factory> svc;

    auto processors = svc.get_processors();
    EXPECT_EQ(processors.size(), 3u);
    EXPECT_EQ(processors[0]->get_index(), 0u);
    EXPECT_EQ(processors[1]->get_index(), 1u);
    EXPECT_EQ(processors[2]->get_index(), 2u);
}

TEST_F(service_test, get_processors_with_filter_applies_filter)
{
    set_up_processors(3);

    service<mock_driver_factory> svc;

    auto filter = [](service<mock_driver_factory>::processor_vector_t& procs) {
        service<mock_driver_factory>::processor_vector_t filtered;
        for(auto& p : procs)
        {
            if(p->get_index() == 1)
            {
                filtered.push_back(p);
            }
        }
        return filtered;
    };

    auto processors = svc.get_processors(filter);
    EXPECT_EQ(processors.size(), 1u);
    EXPECT_EQ(processors[0]->get_index(), 1u);
}

TEST_F(service_test, shutdown_calls_driver_shutdown)
{
    service<mock_driver_factory> svc;

    EXPECT_CALL(*m_mock_driver, shutdown()).Times(1);

    svc.shutdown();
}

TEST_F(service_test, processor_types_are_set_correctly)
{
    set_up_processors(2);

    ON_CALL(*m_mock_driver, get_processor_type(_, _))
        .WillByDefault([](amdsmi_processor_handle handle, processor_type_t* type) {
            auto addr = reinterpret_cast<uintptr_t>(handle);
            if(addr == 0x1000)
            {
                *type = AMDSMI_PROCESSOR_TYPE_AMD_GPU;
            }
            else
            {
                *type = AMDSMI_PROCESSOR_TYPE_AMD_CPU;
            }
            return AMDSMI_STATUS_SUCCESS;
        });

    service<mock_driver_factory> svc;

    auto processors = svc.get_processors();
    EXPECT_EQ(processors[0]->get_processor_type(), AMDSMI_PROCESSOR_TYPE_AMD_GPU);
    EXPECT_EQ(processors[1]->get_processor_type(), AMDSMI_PROCESSOR_TYPE_AMD_CPU);
}

}  // namespace testing
}  // namespace amd_smi
}  // namespace rocprofsys

#endif  // ROCPROFSYS_USE_ROCM > 0
