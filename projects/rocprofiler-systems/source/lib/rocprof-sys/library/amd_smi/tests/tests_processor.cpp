// Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// MIT License - See LICENSE file for details.

#include "library/amd_smi/processor.hpp"
#include "mock_driver.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sstream>

#if ROCPROFSYS_USE_ROCM > 0

namespace rocprofsys
{
namespace amd_smi
{
namespace testing
{

class ProcessorTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_mock_driver = std::make_shared<::testing::NiceMock<mock_driver>>();
        m_mock_driver->set_up_defaults();
    }

    void TearDown() override { m_mock_driver.reset(); }

    std::shared_ptr<::testing::NiceMock<mock_driver>> m_mock_driver;
};

TEST_F(ProcessorTest, ConstructorInitializesFields)
{
    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);
    processor_type_t        type   = AMDSMI_PROCESSOR_TYPE_AMD_GPU;
    size_t                  index  = 0;

    processor<mock_driver> proc(m_mock_driver, handle, type, index);

    EXPECT_EQ(proc.get_handle(), handle);
    EXPECT_EQ(proc.get_processor_type(), type);
    EXPECT_EQ(proc.get_index(), index);
    EXPECT_TRUE(proc.is_enabled());
    EXPECT_FALSE(proc.is_disabled_due_to_error());
}

TEST_F(ProcessorTest, ConstructorWithDifferentProcessorTypes)
{
    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    processor<mock_driver> gpu_proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU,
                                    0);
    EXPECT_EQ(gpu_proc.get_processor_type(), AMDSMI_PROCESSOR_TYPE_AMD_GPU);

    processor<mock_driver> cpu_proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_CPU,
                                    1);
    EXPECT_EQ(cpu_proc.get_processor_type(), AMDSMI_PROCESSOR_TYPE_AMD_CPU);
}

TEST_F(ProcessorTest, SetEnabledChangesState)
{
    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);
    processor<mock_driver>  proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    EXPECT_TRUE(proc.is_enabled());

    proc.set_enabled(false);
    EXPECT_FALSE(proc.is_enabled());

    proc.set_enabled(true);
    EXPECT_TRUE(proc.is_enabled());
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsValidActivityMetrics)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_engine_usage_t expected_activity{};
    expected_activity.gfx_activity = 50;
    expected_activity.umc_activity = 30;
    expected_activity.mm_activity  = 20;

    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillOnce(
            DoAll(SetArgPointee<1>(expected_activity), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.gfx_activity, 50u);
    EXPECT_EQ(metrics.umc_activity, 30u);
    EXPECT_EQ(metrics.mm_activity, 20u);
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsPowerMetrics)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_power_info_t expected_power{};
    expected_power.current_socket_power = 150000;
    expected_power.average_socket_power = 140000;

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(expected_power), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.current_socket_power, 150000u);
    EXPECT_EQ(metrics.average_socket_power, 140000u);
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsTemperatureMetrics)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    EXPECT_CALL(*m_mock_driver,
                get_temperature_metric(handle, AMDSMI_TEMPERATURE_TYPE_HOTSPOT,
                                       AMDSMI_TEMP_CURRENT, _))
        .WillRepeatedly(DoAll(SetArgPointee<3>(75000), Return(AMDSMI_STATUS_SUCCESS)));

    EXPECT_CALL(*m_mock_driver,
                get_temperature_metric(handle, AMDSMI_TEMPERATURE_TYPE_EDGE,
                                       AMDSMI_TEMP_CURRENT, _))
        .WillRepeatedly(DoAll(SetArgPointee<3>(65000), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.hotspot_temperature, 75000);
    EXPECT_EQ(metrics.edge_temperature, 65000);
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsMemoryMetrics)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    uint64_t expected_mem_usage = 4096000000ULL;

    EXPECT_CALL(*m_mock_driver, get_memory_usage(handle, AMDSMI_MEM_TYPE_VRAM, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<2>(expected_mem_usage), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.memory_usage, expected_mem_usage);
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsGpuMetricsPcie)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t expected_gpu_metrics{};
    expected_gpu_metrics.pcie_link_width     = 16;
    expected_gpu_metrics.pcie_link_speed     = 5000;
    expected_gpu_metrics.pcie_bandwidth_acc  = 1000000ULL;
    expected_gpu_metrics.pcie_bandwidth_inst = 50000ULL;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(expected_gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.pcie_link_width, 16u);
    EXPECT_EQ(metrics.pcie_link_speed, 5000u);
    EXPECT_EQ(metrics.pcie_bandwidth_acc, 1000000ULL);
    EXPECT_EQ(metrics.pcie_bandwidth_inst, 50000ULL);
}

TEST_F(ProcessorTest, GetSmiMetricsReturnsGpuMetricsXgmi)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t expected_gpu_metrics{};
    expected_gpu_metrics.xgmi_link_width        = 8;
    expected_gpu_metrics.xgmi_link_speed        = 25000;
    expected_gpu_metrics.xgmi_read_data_acc[0]  = 500000ULL;
    expected_gpu_metrics.xgmi_write_data_acc[0] = 600000ULL;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(expected_gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.xgmi_link_width, 8u);
    EXPECT_EQ(metrics.xgmi_link_speed, 25000u);
    EXPECT_EQ(metrics.xgmi_read_data_acc[0], 500000ULL);
    EXPECT_EQ(metrics.xgmi_write_data_acc[0], 600000ULL);
}

TEST_F(ProcessorTest, GetSmiMetricsHandlesNotSupportedValues)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics_with_unsupported{};
    gpu_metrics_with_unsupported.pcie_link_width     = UINT16_MAX;
    gpu_metrics_with_unsupported.pcie_link_speed     = UINT16_MAX;
    gpu_metrics_with_unsupported.pcie_bandwidth_acc  = UINT64_MAX;
    gpu_metrics_with_unsupported.pcie_bandwidth_inst = UINT64_MAX;
    gpu_metrics_with_unsupported.xgmi_link_width     = UINT16_MAX;
    gpu_metrics_with_unsupported.xgmi_link_speed     = UINT16_MAX;
    for(auto& val : gpu_metrics_with_unsupported.xgmi_read_data_acc)
    {
        val = UINT64_MAX;
    }
    for(auto& val : gpu_metrics_with_unsupported.xgmi_write_data_acc)
    {
        val = UINT64_MAX;
    }

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(gpu_metrics_with_unsupported),
                              Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_FALSE(supported.bits.pcie);
    EXPECT_FALSE(supported.bits.xgmi);
}

TEST_F(ProcessorTest, GetSmiMetricsHandlesNotSupported)
{
    using ::testing::_;
    using ::testing::Return;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_temperature_metric(handle, _, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_memory_usage(handle, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();
    EXPECT_EQ(metrics.gfx_activity, 0u);
    EXPECT_EQ(metrics.memory_usage, 0u);
}

TEST_F(ProcessorTest, GetSupportedMetricsReturnsCorrectFlags)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_power_info_t power_info{};
    power_info.current_socket_power = 150000;
    power_info.average_socket_power = 140000;

    amdsmi_engine_usage_t activity{};
    activity.gfx_activity = 50;

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(power_info), Return(AMDSMI_STATUS_SUCCESS)));
    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(activity), Return(AMDSMI_STATUS_SUCCESS)));
    EXPECT_CALL(*m_mock_driver, get_memory_usage(handle, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_SUCCESS));
    EXPECT_CALL(*m_mock_driver, get_temperature_metric(handle, _, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_SUCCESS));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_TRUE(supported.bits.current_socket_power);
    EXPECT_TRUE(supported.bits.average_socket_power);
    EXPECT_TRUE(supported.bits.gfx_activity);
    EXPECT_TRUE(supported.bits.memory_usage);
    EXPECT_TRUE(supported.bits.hotspot_temperature);
    EXPECT_TRUE(supported.bits.edge_temperature);
}

TEST_F(ProcessorTest, GetSupportedMetricsWhenPowerNotSupported)
{
    using ::testing::_;
    using ::testing::Return;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_memory_usage(handle, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_temperature_metric(handle, _, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));
    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_FALSE(supported.bits.current_socket_power);
    EXPECT_FALSE(supported.bits.average_socket_power);
    EXPECT_FALSE(supported.bits.gfx_activity);
    EXPECT_FALSE(supported.bits.umc_activity);
    EXPECT_FALSE(supported.bits.mm_activity);
    EXPECT_FALSE(supported.bits.memory_usage);
    EXPECT_FALSE(supported.bits.hotspot_temperature);
    EXPECT_FALSE(supported.bits.edge_temperature);
}

TEST_F(ProcessorTest, GetSupportedMetricsWithMetricValueNotSupported)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_power_info_t power_info{};
    power_info.current_socket_power = METRIC_VALUE_NOT_SUPPORTED;
    power_info.average_socket_power = METRIC_VALUE_NOT_SUPPORTED;

    amdsmi_engine_usage_t activity{};
    activity.gfx_activity = METRIC_VALUE_NOT_SUPPORTED;

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(power_info), Return(AMDSMI_STATUS_SUCCESS)));
    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(activity), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_FALSE(supported.bits.current_socket_power);
    EXPECT_FALSE(supported.bits.average_socket_power);
    EXPECT_FALSE(supported.bits.gfx_activity);
}

TEST_F(ProcessorTest, MultipleProcessorsHaveDifferentIndices)
{
    amdsmi_processor_handle handle1 = reinterpret_cast<amdsmi_processor_handle>(0x1);
    amdsmi_processor_handle handle2 = reinterpret_cast<amdsmi_processor_handle>(0x2);
    amdsmi_processor_handle handle3 = reinterpret_cast<amdsmi_processor_handle>(0x3);

    processor<mock_driver> proc1(m_mock_driver, handle1, AMDSMI_PROCESSOR_TYPE_AMD_GPU,
                                 0);
    processor<mock_driver> proc2(m_mock_driver, handle2, AMDSMI_PROCESSOR_TYPE_AMD_GPU,
                                 1);
    processor<mock_driver> proc3(m_mock_driver, handle3, AMDSMI_PROCESSOR_TYPE_AMD_GPU,
                                 2);

    EXPECT_EQ(proc1.get_index(), 0u);
    EXPECT_EQ(proc2.get_index(), 1u);
    EXPECT_EQ(proc3.get_index(), 2u);
}

TEST_F(ProcessorTest, PrintSupportedMetricsOutputsCorrectly)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_power_info_t power_info{};
    power_info.current_socket_power = 150000;
    power_info.average_socket_power = 140000;

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(power_info), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    std::stringstream buffer;
    std::streambuf*   old_cout = std::cout.rdbuf(buffer.rdbuf());

    proc.print_supported_metrics();

    std::cout.rdbuf(old_cout);

    std::string output = buffer.str();
    EXPECT_NE(output.find("SUPPORTED SMI METRICS"), std::string::npos);
    EXPECT_NE(output.find("Processor 0"), std::string::npos);
    EXPECT_NE(output.find("current_socket_power"), std::string::npos);
    EXPECT_NE(output.find("average_socket_power"), std::string::npos);
}

TEST_F(ProcessorTest, VcnActivityMetricsSupported)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xcp_stats[0].vcn_busy[0] = 50;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.vcn_activity);
}

TEST_F(ProcessorTest, JpegActivityMetricsSupported)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xcp_stats[0].jpeg_busy[0] = 75;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.jpeg_activity);
}

TEST_F(ProcessorTest, VcnAndJpegMetricsNotSupportedWithMaxValue)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    for(auto& xcp : gpu_metrics.xcp_stats)
    {
        for(auto& val : xcp.vcn_busy)
        {
            val = METRIC_VALUE_NOT_SUPPORTED;
        }
        for(auto& val : xcp.jpeg_busy)
        {
            val = METRIC_VALUE_NOT_SUPPORTED;
        }
    }

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_FALSE(supported.bits.vcn_activity);
    EXPECT_FALSE(supported.bits.jpeg_activity);
}

TEST_F(ProcessorTest, GetSmiMetricsCollectsVcnActivity)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xcp_stats[0].vcn_busy[0] = 50;
    gpu_metrics.xcp_stats[0].vcn_busy[1] = 60;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.xcp_stats[0].vcn_busy[0], 50u);
    EXPECT_EQ(metrics.xcp_stats[0].vcn_busy[1], 60u);
}

TEST_F(ProcessorTest, GetSmiMetricsCollectsJpegActivity)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xcp_stats[0].jpeg_busy[0] = 75;
    gpu_metrics.xcp_stats[0].jpeg_busy[1] = 80;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.xcp_stats[0].jpeg_busy[0], 75u);
    EXPECT_EQ(metrics.xcp_stats[0].jpeg_busy[1], 80u);
}

TEST_F(ProcessorTest, XgmiMetricsSupportedWhenLinkWidthValid)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xgmi_link_width = 8;
    gpu_metrics.xgmi_link_speed = UINT16_MAX;
    for(auto& val : gpu_metrics.xgmi_read_data_acc)
    {
        val = UINT64_MAX;
    }

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.xgmi);
}

TEST_F(ProcessorTest, XgmiMetricsSupportedWhenReadDataValid)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xgmi_link_width = UINT16_MAX;
    gpu_metrics.xgmi_link_speed = UINT16_MAX;
    for(auto& val : gpu_metrics.xgmi_read_data_acc)
    {
        val = UINT64_MAX;
    }
    gpu_metrics.xgmi_read_data_acc[0] = 1000;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.xgmi);
}

TEST_F(ProcessorTest, PcieMetricsSupportedWhenLinkWidthValid)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.pcie_link_width     = 16;
    gpu_metrics.pcie_link_speed     = UINT16_MAX;
    gpu_metrics.pcie_bandwidth_acc  = UINT64_MAX;
    gpu_metrics.pcie_bandwidth_inst = UINT64_MAX;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.pcie);
}

TEST_F(ProcessorTest, PcieMetricsSupportedWhenBandwidthAccValid)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.pcie_link_width     = UINT16_MAX;
    gpu_metrics.pcie_link_speed     = UINT16_MAX;
    gpu_metrics.pcie_bandwidth_acc  = 500000ULL;
    gpu_metrics.pcie_bandwidth_inst = UINT64_MAX;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();
    EXPECT_TRUE(supported.bits.pcie);
}

TEST_F(ProcessorTest, ActivityMetricsFailedDuringCollection)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_engine_usage_t init_activity{};
    init_activity.gfx_activity = 50;

    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillOnce(DoAll(SetArgPointee<1>(init_activity), Return(AMDSMI_STATUS_SUCCESS)))
        .WillRepeatedly(Return(AMDSMI_STATUS_IO));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.gfx_activity, 0u);
}

TEST_F(ProcessorTest, PowerMetricsFailedDuringCollection)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_power_info_t init_power{};
    init_power.current_socket_power = 150000;
    init_power.average_socket_power = 140000;

    EXPECT_CALL(*m_mock_driver, get_power_info(handle, _))
        .WillOnce(DoAll(SetArgPointee<1>(init_power), Return(AMDSMI_STATUS_SUCCESS)))
        .WillRepeatedly(Return(AMDSMI_STATUS_IO));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    EXPECT_EQ(metrics.current_socket_power, 0u);
    EXPECT_EQ(metrics.average_socket_power, 0u);
}

TEST_F(ProcessorTest, GetSmiMetricsWithAllXgmiLinks)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};
    gpu_metrics.xgmi_link_width = 8;
    gpu_metrics.xgmi_link_speed = 25000;

    for(size_t i = 0; i < AMDSMI_MAX_NUM_XGMI_LINKS; ++i)
    {
        gpu_metrics.xgmi_read_data_acc[i]  = (i + 1) * 1000;
        gpu_metrics.xgmi_write_data_acc[i] = (i + 1) * 2000;
    }

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    for(size_t i = 0; i < AMDSMI_MAX_NUM_XGMI_LINKS; ++i)
    {
        EXPECT_EQ(metrics.xgmi_read_data_acc[i], (i + 1) * 1000);
        EXPECT_EQ(metrics.xgmi_write_data_acc[i], (i + 1) * 2000);
    }
}

TEST_F(ProcessorTest, GetSmiMetricsWithMultipleXcpStats)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_gpu_metrics_t gpu_metrics{};

    for(size_t xcp = 0; xcp < AMDSMI_MAX_NUM_XCP; ++xcp)
    {
        gpu_metrics.xcp_stats[xcp].vcn_busy[0]  = static_cast<uint16_t>(xcp * 10);
        gpu_metrics.xcp_stats[xcp].jpeg_busy[0] = static_cast<uint16_t>(xcp * 20);
    }

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto metrics = proc.get_smi_metrics();

    for(size_t xcp = 0; xcp < AMDSMI_MAX_NUM_XCP; ++xcp)
    {
        EXPECT_EQ(metrics.xcp_stats[xcp].vcn_busy[0], xcp * 10);
        EXPECT_EQ(metrics.xcp_stats[xcp].jpeg_busy[0], xcp * 20);
    }
}

TEST_F(ProcessorTest, TemperatureMetricReturnedAsNotSupportedValue)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    EXPECT_CALL(*m_mock_driver, get_temperature_metric(handle, _, _, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<3>(static_cast<int64_t>(METRIC_VALUE_NOT_SUPPORTED)),
                  Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_FALSE(supported.bits.hotspot_temperature);
    EXPECT_FALSE(supported.bits.edge_temperature);
}

TEST_F(ProcessorTest, EdgeTemperatureSupportedButHotspotNot)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    EXPECT_CALL(*m_mock_driver,
                get_temperature_metric(handle, AMDSMI_TEMPERATURE_TYPE_HOTSPOT, _, _))
        .WillRepeatedly(Return(AMDSMI_STATUS_NOT_SUPPORTED));

    EXPECT_CALL(*m_mock_driver,
                get_temperature_metric(handle, AMDSMI_TEMPERATURE_TYPE_EDGE, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<3>(65000), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_FALSE(supported.bits.hotspot_temperature);
    EXPECT_TRUE(supported.bits.edge_temperature);
}

TEST_F(ProcessorTest, SharedDriverAcrossMultipleProcessors)
{
    amdsmi_processor_handle handle1 = reinterpret_cast<amdsmi_processor_handle>(0x1);
    amdsmi_processor_handle handle2 = reinterpret_cast<amdsmi_processor_handle>(0x2);

    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, handle1, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc2 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, handle2, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);

    EXPECT_EQ(proc1->get_index(), 0u);
    EXPECT_EQ(proc2->get_index(), 1u);

    EXPECT_EQ(proc1->get_handle(), handle1);
    EXPECT_EQ(proc2->get_handle(), handle2);
}

TEST_F(ProcessorTest, UmcAndMmActivityAlwaysSupportedWhenActivitySucceeds)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle = reinterpret_cast<amdsmi_processor_handle>(0x1234);

    amdsmi_engine_usage_t activity{};
    activity.gfx_activity = METRIC_VALUE_NOT_SUPPORTED;
    activity.umc_activity = 50;
    activity.mm_activity  = 60;

    EXPECT_CALL(*m_mock_driver, get_activity(handle, _))
        .WillRepeatedly(DoAll(SetArgPointee<1>(activity), Return(AMDSMI_STATUS_SUCCESS)));

    processor<mock_driver> proc(m_mock_driver, handle, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);

    auto supported = proc.get_supported_metrics();

    EXPECT_FALSE(supported.bits.gfx_activity);
    EXPECT_TRUE(supported.bits.umc_activity);
    EXPECT_TRUE(supported.bits.mm_activity);
}

}  // namespace testing
}  // namespace amd_smi
}  // namespace rocprofsys

#endif  // ROCPROFSYS_USE_ROCM > 0
