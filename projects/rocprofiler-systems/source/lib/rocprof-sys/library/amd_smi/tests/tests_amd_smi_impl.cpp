// Copyright (c) 2018-2025 Advanced Micro Devices, Inc. All Rights Reserved.
// MIT License - See LICENSE file for details.

#include "library/amd_smi/amd_smi_impl.hpp"
#include "library/amd_smi/common.hpp"
#include "library/amd_smi/processor.hpp"
#include "library/amd_smi/service.hpp"
#include "mock_driver.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <vector>

#if ROCPROFSYS_USE_ROCM > 0

namespace rocprofsys
{
namespace amd_smi
{
namespace testing
{

struct mock_settings_policy
{
    static device_filter  s_device_filter;
    static enabled_metric s_enabled_metrics;
    static bool           s_use_perfetto_legacy_metrics;

    static device_filter  get_device_filter() { return s_device_filter; }
    static enabled_metric get_enabled_metrics() { return s_enabled_metrics; }
    static bool           get_use_perfetto_legacy_metrics()
    {
        return s_use_perfetto_legacy_metrics;
    }

    static void reset()
    {
        s_device_filter               = { device_selection_mode::ALL, {} };
        s_enabled_metrics             = { .value = 0xFFFF };
        s_use_perfetto_legacy_metrics = true;
    }
};

device_filter  mock_settings_policy::s_device_filter = { device_selection_mode::ALL, {} };
enabled_metric mock_settings_policy::s_enabled_metrics             = { .value = 0xFFFF };
bool           mock_settings_policy::s_use_perfetto_legacy_metrics = true;

struct mock_perfetto_policy
{
    struct perfetto_sample_data
    {
        size_t        device_index;
        smi_metrics   metrics;
        unsigned long timestamp;
    };

    static std::vector<size_t>                      s_initialized_devices;
    static std::vector<std::pair<size_t, uint64_t>> s_stored_samples;
    static std::vector<perfetto_sample_data>        s_sample_data;
    static bool                                     s_post_processed;

    template <typename ProcessorVector>
    static void init_storage(const ProcessorVector& processors)
    {
        for(const auto& processor : processors)
        {
            s_initialized_devices.push_back(processor->get_index());
        }
    }

    static void setup_counter_tracks(
        [[maybe_unused]] size_t                device_index,
        [[maybe_unused]] const enabled_metric& enabled_metrics)
    {}

    static void store_sample(size_t device_index, const smi_metrics& metrics,
                             unsigned long timestamp)
    {
        s_stored_samples.emplace_back(device_index, timestamp);
        s_sample_data.push_back({ device_index, metrics, timestamp });
    }

    template <typename ProcessorVector>
    static void post_process([[maybe_unused]] const ProcessorVector& processors,
                             [[maybe_unused]] enabled_metric         enabled_metrics)
    {
        s_post_processed = true;
    }

    static void reset()
    {
        s_initialized_devices.clear();
        s_stored_samples.clear();
        s_sample_data.clear();
        s_post_processed = false;
    }
};

std::vector<size_t>                      mock_perfetto_policy::s_initialized_devices;
std::vector<std::pair<size_t, uint64_t>> mock_perfetto_policy::s_stored_samples;
std::vector<mock_perfetto_policy::perfetto_sample_data>
     mock_perfetto_policy::s_sample_data;
bool mock_perfetto_policy::s_post_processed = false;

struct mock_rocpd_policy
{
    struct cache_sample_data
    {
        size_t         device_id;
        enabled_metric supported;
        enabled_metric enabled;
        smi_metrics    metrics;
        unsigned long  timestamp;
    };

    static bool                           s_category_initialized;
    static bool                           s_tracks_initialized;
    static bool                           s_pmc_initialized;
    static std::vector<cache_sample_data> s_sample_data;

    static void initialize_category_metadata() { s_category_initialized = true; }
    static void initialize_smi_tracks_metadata([[maybe_unused]] size_t gpu_id)
    {
        s_tracks_initialized = true;
    }
    static void initialize_smi_pmc_metadata([[maybe_unused]] size_t gpu_id)
    {
        s_pmc_initialized = true;
    }
    static void store_sample(size_t device_id, const enabled_metric& supported,
                             const enabled_metric& enabled, const smi_metrics& metrics,
                             unsigned long timestamp)
    {
        s_sample_data.push_back({ device_id, supported, enabled, metrics, timestamp });
    }

    static void reset()
    {
        s_category_initialized = false;
        s_tracks_initialized   = false;
        s_pmc_initialized      = false;
        s_sample_data.clear();
    }
};

bool mock_rocpd_policy::s_category_initialized = false;
bool mock_rocpd_policy::s_tracks_initialized   = false;
bool mock_rocpd_policy::s_pmc_initialized      = false;
std::vector<mock_rocpd_policy::cache_sample_data> mock_rocpd_policy::s_sample_data;

class mock_service
{
public:
    using processor_t        = processor<mock_driver>;
    using processor_ptr_t    = std::shared_ptr<processor_t>;
    using processor_vector_t = std::vector<processor_ptr_t>;
    using filter_func_t = std::function<processor_vector_t(const processor_vector_t&)>;

    static processor_vector_t s_processors;
    static version            s_version;

    const version& get_version() const { return s_version; }

    processor_vector_t get_processors(const filter_func_t& filter = nullptr)
    {
        if(filter)
        {
            return filter(s_processors);
        }
        return s_processors;
    }

    void shutdown() {}

    static void reset()
    {
        s_processors.clear();
        s_version = {};
    }

    static void add_processor(processor_ptr_t proc)
    {
        s_processors.push_back(std::move(proc));
    }
};

mock_service::processor_vector_t mock_service::s_processors;
version                          mock_service::s_version = { { 1, 0, 0 }, "test" };

struct mock_service_factory
{
    using smi_service        = mock_service;
    using processor_t        = typename smi_service::processor_t;
    using processor_vector_t = typename smi_service::processor_vector_t;

    static std::shared_ptr<smi_service> create_smi_service()
    {
        return std::make_shared<smi_service>();
    }
};

struct test_config
{
    using SmiServiceFactory = mock_service_factory;
    using SettingsApi       = mock_settings_policy;
    using PerfettoApi       = mock_perfetto_policy;
    using RocpdApi          = mock_rocpd_policy;
};

class AmdSmiImplTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        mock_settings_policy::reset();
        mock_perfetto_policy::reset();
        mock_rocpd_policy::reset();
        mock_service::reset();

        m_mock_driver = std::make_shared<::testing::NiceMock<mock_driver>>();
        m_mock_driver->set_up_defaults();
    }

    void TearDown() override
    {
        mock_service::reset();
        m_mock_driver.reset();
    }

    std::shared_ptr<::testing::NiceMock<mock_driver>> m_mock_driver;
};

TEST_F(AmdSmiImplTest, SetupInitializesService)
{
    amd_smi_impl<test_config> impl;

    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 0u);
}

TEST_F(AmdSmiImplTest, SetupWithProcessorsInitializesPerfetto)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 1u);
    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices.size(), 1u);
    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices[0], 0u);
}

TEST_F(AmdSmiImplTest, ConfigInitializesMetadata)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    amd_smi_impl<test_config> impl;
    impl.setup();
    impl.config();

    EXPECT_TRUE(mock_rocpd_policy::s_category_initialized);
    EXPECT_TRUE(mock_rocpd_policy::s_tracks_initialized);
    EXPECT_TRUE(mock_rocpd_policy::s_pmc_initialized);
}

TEST_F(AmdSmiImplTest, SampleStoresData)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    amd_smi_impl<test_config> impl;
    impl.setup();

    uint64_t test_timestamp = 1000000;
    impl.sample([test_timestamp]() { return test_timestamp; });

    EXPECT_EQ(mock_perfetto_policy::s_stored_samples.size(), 1u);
    EXPECT_EQ(mock_perfetto_policy::s_stored_samples[0].first, 0u);
    EXPECT_EQ(mock_perfetto_policy::s_stored_samples[0].second, test_timestamp);
}

TEST_F(AmdSmiImplTest, PostProcessCallsPerfettoPostProcess)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    amd_smi_impl<test_config> impl;
    impl.setup();
    impl.post_process();

    EXPECT_TRUE(mock_perfetto_policy::s_post_processed);
}

TEST_F(AmdSmiImplTest, DeviceFilterNoneReturnsNoProcessors)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    mock_settings_policy::s_device_filter = { device_selection_mode::NONE, {} };

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 0u);
}

TEST_F(AmdSmiImplTest, DeviceFilterSpecificReturnsSelectedProcessors)
{
    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x2),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);
    auto proc2 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x3),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 2);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);
    mock_service::add_processor(proc2);

    mock_settings_policy::s_device_filter = { device_selection_mode::SPECIFIC, { 0, 2 } };

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 2u);

    const auto& processors = impl.get_processors();
    EXPECT_EQ(processors[0]->get_index(), 0u);
    EXPECT_EQ(processors[1]->get_index(), 2u);
}

TEST_F(AmdSmiImplTest, SetupSkipsPerfettoWhenLegacyMetricsDisabled)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    mock_settings_policy::s_use_perfetto_legacy_metrics = false;

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 1u);
    EXPECT_TRUE(mock_perfetto_policy::s_initialized_devices.empty());
}

TEST_F(AmdSmiImplTest, ConfigSkipsPerfettoSetupWhenLegacyMetricsDisabled)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    mock_settings_policy::s_use_perfetto_legacy_metrics = false;

    amd_smi_impl<test_config> impl;
    impl.setup();
    impl.config();

    EXPECT_TRUE(mock_rocpd_policy::s_category_initialized);
    EXPECT_TRUE(mock_rocpd_policy::s_tracks_initialized);
    EXPECT_TRUE(mock_rocpd_policy::s_pmc_initialized);
}

TEST_F(AmdSmiImplTest, SampleSkipsPerfettoWhenLegacyMetricsDisabled)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    mock_settings_policy::s_use_perfetto_legacy_metrics = false;

    amd_smi_impl<test_config> impl;
    impl.setup();

    uint64_t test_timestamp = 1000000;
    impl.sample([test_timestamp]() { return test_timestamp; });

    EXPECT_TRUE(mock_perfetto_policy::s_stored_samples.empty());
}

TEST_F(AmdSmiImplTest, PostProcessSkipsPerfettoWhenLegacyMetricsDisabled)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    mock_settings_policy::s_use_perfetto_legacy_metrics = false;

    amd_smi_impl<test_config> impl;
    impl.setup();
    impl.post_process();

    EXPECT_FALSE(mock_perfetto_policy::s_post_processed);
}

TEST_F(AmdSmiImplTest, ShutdownCleansUpService)
{
    auto proc = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    mock_service::add_processor(proc);

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 1u);

    impl.shutdown();
}

TEST_F(AmdSmiImplTest, GetProcessorsReturnsCorrectList)
{
    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x2),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);

    amd_smi_impl<test_config> impl;
    impl.setup();

    const auto& processors = impl.get_processors();
    EXPECT_EQ(processors.size(), 2u);
    EXPECT_EQ(processors[0]->get_index(), 0u);
    EXPECT_EQ(processors[1]->get_index(), 1u);
}

TEST_F(AmdSmiImplTest, DeviceFilterAllReturnsAllProcessors)
{
    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x2),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);
    auto proc2 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x3),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 2);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);
    mock_service::add_processor(proc2);

    mock_settings_policy::s_device_filter = { device_selection_mode::ALL, {} };

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(impl.get_processor_count(), 3u);
}

TEST_F(AmdSmiImplTest, SampleWithMultipleProcessors)
{
    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x2),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);

    amd_smi_impl<test_config> impl;
    impl.setup();

    uint64_t test_timestamp = 1000000;
    impl.sample([test_timestamp]() { return test_timestamp; });

    EXPECT_EQ(mock_perfetto_policy::s_stored_samples.size(), 2u);
    EXPECT_EQ(mock_perfetto_policy::s_stored_samples[0].first, 0u);
    EXPECT_EQ(mock_perfetto_policy::s_stored_samples[1].first, 1u);
}

TEST_F(AmdSmiImplTest, SetupInitializesPerfettoStorageForAllProcessors)
{
    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x1),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x2),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);
    auto proc2 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, reinterpret_cast<amdsmi_processor_handle>(0x3),
        AMDSMI_PROCESSOR_TYPE_AMD_GPU, 2);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);
    mock_service::add_processor(proc2);

    amd_smi_impl<test_config> impl;
    impl.setup();

    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices.size(), 3u);
    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices[0], 0u);
    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices[1], 1u);
    EXPECT_EQ(mock_perfetto_policy::s_initialized_devices[2], 2u);
}

TEST_F(AmdSmiImplTest, SampleWithTwoProcessorsValidatesAllData)
{
    using ::testing::_;
    using ::testing::DoAll;
    using ::testing::Return;
    using ::testing::SetArgPointee;

    amdsmi_processor_handle handle0 = reinterpret_cast<amdsmi_processor_handle>(0x1);
    amdsmi_processor_handle handle1 = reinterpret_cast<amdsmi_processor_handle>(0x2);

    amdsmi_gpu_metrics_t gpu_metrics0{};
    gpu_metrics0.current_socket_power      = 1500;
    gpu_metrics0.average_socket_power      = 1400;
    gpu_metrics0.average_gfx_activity      = 50;
    gpu_metrics0.average_umc_activity      = 30;
    gpu_metrics0.average_mm_activity       = 25;
    gpu_metrics0.temperature_hotspot       = 75;
    gpu_metrics0.temperature_edge          = 65;
    gpu_metrics0.xgmi_link_width           = 8;
    gpu_metrics0.xgmi_link_speed           = 25000;
    gpu_metrics0.xgmi_read_data_acc[0]     = 100000;
    gpu_metrics0.xgmi_write_data_acc[0]    = 200000;
    gpu_metrics0.pcie_link_width           = 16;
    gpu_metrics0.pcie_link_speed           = 5000;
    gpu_metrics0.pcie_bandwidth_acc        = 500000;
    gpu_metrics0.pcie_bandwidth_inst       = 10000;
    gpu_metrics0.xcp_stats[0].vcn_busy[0]  = 40;
    gpu_metrics0.xcp_stats[0].jpeg_busy[0] = 35;

    amdsmi_gpu_metrics_t gpu_metrics1{};
    gpu_metrics1.current_socket_power      = 2000;
    gpu_metrics1.average_socket_power      = 1900;
    gpu_metrics1.average_gfx_activity      = 80;
    gpu_metrics1.average_umc_activity      = 60;
    gpu_metrics1.average_mm_activity       = 45;
    gpu_metrics1.temperature_hotspot       = 85;
    gpu_metrics1.temperature_edge          = 72;
    gpu_metrics1.xgmi_link_width           = 16;
    gpu_metrics1.xgmi_link_speed           = 32000;
    gpu_metrics1.xgmi_read_data_acc[0]     = 300000;
    gpu_metrics1.xgmi_write_data_acc[0]    = 400000;
    gpu_metrics1.pcie_link_width           = 8;
    gpu_metrics1.pcie_link_speed           = 8000;
    gpu_metrics1.pcie_bandwidth_acc        = 800000;
    gpu_metrics1.pcie_bandwidth_inst       = 20000;
    gpu_metrics1.xcp_stats[0].vcn_busy[0]  = 70;
    gpu_metrics1.xcp_stats[0].jpeg_busy[0] = 55;

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle0, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics0), Return(AMDSMI_STATUS_SUCCESS)));

    EXPECT_CALL(*m_mock_driver, get_metrics_info(handle1, _))
        .WillRepeatedly(
            DoAll(SetArgPointee<1>(gpu_metrics1), Return(AMDSMI_STATUS_SUCCESS)));

    auto proc0 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, handle0, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 0);
    auto proc1 = std::make_shared<processor<mock_driver>>(
        m_mock_driver, handle1, AMDSMI_PROCESSOR_TYPE_AMD_GPU, 1);

    mock_service::add_processor(proc0);
    mock_service::add_processor(proc1);

    amd_smi_impl<test_config> impl;
    impl.setup();

    uint64_t test_timestamp = 1000000;
    impl.sample([test_timestamp]() { return test_timestamp; });

    ASSERT_EQ(mock_perfetto_policy::s_sample_data.size(), 2u);

    const auto& perfetto_sample0 = mock_perfetto_policy::s_sample_data[0];
    EXPECT_EQ(perfetto_sample0.device_index, 0u);
    EXPECT_EQ(perfetto_sample0.timestamp, test_timestamp);
    EXPECT_EQ(perfetto_sample0.metrics.current_socket_power, 1500u);
    EXPECT_EQ(perfetto_sample0.metrics.average_socket_power, 1400u);
    EXPECT_EQ(perfetto_sample0.metrics.gfx_activity, 50u);
    EXPECT_EQ(perfetto_sample0.metrics.umc_activity, 30u);
    EXPECT_EQ(perfetto_sample0.metrics.mm_activity, 25u);
    EXPECT_EQ(perfetto_sample0.metrics.hotspot_temperature, 75);
    EXPECT_EQ(perfetto_sample0.metrics.edge_temperature, 65);
    EXPECT_EQ(perfetto_sample0.metrics.xgmi.link.width, 8u);
    EXPECT_EQ(perfetto_sample0.metrics.xgmi.link.speed, 25000u);
    EXPECT_EQ(perfetto_sample0.metrics.xgmi.data_acc.read[0], 100000u);
    EXPECT_EQ(perfetto_sample0.metrics.xgmi.data_acc.write[0], 200000u);
    EXPECT_EQ(perfetto_sample0.metrics.pcie.link.width, 16u);
    EXPECT_EQ(perfetto_sample0.metrics.pcie.link.speed, 5000u);
    EXPECT_EQ(perfetto_sample0.metrics.pcie.bandwidth.acc, 500000u);
    EXPECT_EQ(perfetto_sample0.metrics.pcie.bandwidth.inst, 10000u);
    EXPECT_EQ(perfetto_sample0.metrics.xcp_stats[0].vcn_busy[0], 40u);
    EXPECT_EQ(perfetto_sample0.metrics.xcp_stats[0].jpeg_busy[0], 35u);

    const auto& perfetto_sample1 = mock_perfetto_policy::s_sample_data[1];
    EXPECT_EQ(perfetto_sample1.device_index, 1u);
    EXPECT_EQ(perfetto_sample1.timestamp, test_timestamp);
    EXPECT_EQ(perfetto_sample1.metrics.current_socket_power, 2000u);
    EXPECT_EQ(perfetto_sample1.metrics.average_socket_power, 1900u);
    EXPECT_EQ(perfetto_sample1.metrics.gfx_activity, 80u);
    EXPECT_EQ(perfetto_sample1.metrics.umc_activity, 60u);
    EXPECT_EQ(perfetto_sample1.metrics.mm_activity, 45u);
    EXPECT_EQ(perfetto_sample1.metrics.hotspot_temperature, 85);
    EXPECT_EQ(perfetto_sample1.metrics.edge_temperature, 72);
    EXPECT_EQ(perfetto_sample1.metrics.xgmi.link.width, 16u);
    EXPECT_EQ(perfetto_sample1.metrics.xgmi.link.speed, 32000u);
    EXPECT_EQ(perfetto_sample1.metrics.xgmi.data_acc.read[0], 300000u);
    EXPECT_EQ(perfetto_sample1.metrics.xgmi.data_acc.write[0], 400000u);
    EXPECT_EQ(perfetto_sample1.metrics.pcie.link.width, 8u);
    EXPECT_EQ(perfetto_sample1.metrics.pcie.link.speed, 8000u);
    EXPECT_EQ(perfetto_sample1.metrics.pcie.bandwidth.acc, 800000u);
    EXPECT_EQ(perfetto_sample1.metrics.pcie.bandwidth.inst, 20000u);
    EXPECT_EQ(perfetto_sample1.metrics.xcp_stats[0].vcn_busy[0], 70u);
    EXPECT_EQ(perfetto_sample1.metrics.xcp_stats[0].jpeg_busy[0], 55u);

    ASSERT_EQ(mock_rocpd_policy::s_sample_data.size(), 2u);

    const auto& cache_sample0 = mock_rocpd_policy::s_sample_data[0];
    EXPECT_EQ(cache_sample0.device_id, 0u);
    EXPECT_EQ(cache_sample0.timestamp, test_timestamp);
    EXPECT_TRUE(cache_sample0.supported.current_socket_power);
    EXPECT_TRUE(cache_sample0.supported.average_socket_power);
    EXPECT_TRUE(cache_sample0.supported.gfx_activity);
    EXPECT_TRUE(cache_sample0.supported.umc_activity);
    EXPECT_TRUE(cache_sample0.supported.mm_activity);
    EXPECT_TRUE(cache_sample0.supported.hotspot_temperature);
    EXPECT_TRUE(cache_sample0.supported.edge_temperature);
    EXPECT_TRUE(cache_sample0.supported.xgmi);
    EXPECT_TRUE(cache_sample0.supported.pcie);
    EXPECT_TRUE(cache_sample0.supported.vcn_activity);
    EXPECT_TRUE(cache_sample0.supported.jpeg_activity);
    EXPECT_EQ(cache_sample0.metrics.current_socket_power, 1500u);
    EXPECT_EQ(cache_sample0.metrics.average_socket_power, 1400u);
    EXPECT_EQ(cache_sample0.metrics.gfx_activity, 50u);
    EXPECT_EQ(cache_sample0.metrics.umc_activity, 30u);
    EXPECT_EQ(cache_sample0.metrics.mm_activity, 25u);
    EXPECT_EQ(cache_sample0.metrics.hotspot_temperature, 75);
    EXPECT_EQ(cache_sample0.metrics.edge_temperature, 65);
    EXPECT_EQ(cache_sample0.metrics.xgmi.link.width, 8u);
    EXPECT_EQ(cache_sample0.metrics.xgmi.link.speed, 25000u);
    EXPECT_EQ(cache_sample0.metrics.xgmi.data_acc.read[0], 100000u);
    EXPECT_EQ(cache_sample0.metrics.xgmi.data_acc.write[0], 200000u);
    EXPECT_EQ(cache_sample0.metrics.pcie.link.width, 16u);
    EXPECT_EQ(cache_sample0.metrics.pcie.link.speed, 5000u);
    EXPECT_EQ(cache_sample0.metrics.pcie.bandwidth.acc, 500000u);
    EXPECT_EQ(cache_sample0.metrics.pcie.bandwidth.inst, 10000u);
    EXPECT_EQ(cache_sample0.metrics.xcp_stats[0].vcn_busy[0], 40u);
    EXPECT_EQ(cache_sample0.metrics.xcp_stats[0].jpeg_busy[0], 35u);

    const auto& cache_sample1 = mock_rocpd_policy::s_sample_data[1];
    EXPECT_EQ(cache_sample1.device_id, 1u);
    EXPECT_EQ(cache_sample1.timestamp, test_timestamp);
    EXPECT_TRUE(cache_sample1.supported.current_socket_power);
    EXPECT_TRUE(cache_sample1.supported.average_socket_power);
    EXPECT_TRUE(cache_sample1.supported.gfx_activity);
    EXPECT_TRUE(cache_sample1.supported.umc_activity);
    EXPECT_TRUE(cache_sample1.supported.mm_activity);
    EXPECT_TRUE(cache_sample1.supported.hotspot_temperature);
    EXPECT_TRUE(cache_sample1.supported.edge_temperature);
    EXPECT_TRUE(cache_sample1.supported.xgmi);
    EXPECT_TRUE(cache_sample1.supported.pcie);
    EXPECT_TRUE(cache_sample1.supported.vcn_activity);
    EXPECT_TRUE(cache_sample1.supported.jpeg_activity);
    EXPECT_EQ(cache_sample1.metrics.current_socket_power, 2000u);
    EXPECT_EQ(cache_sample1.metrics.average_socket_power, 1900u);
    EXPECT_EQ(cache_sample1.metrics.gfx_activity, 80u);
    EXPECT_EQ(cache_sample1.metrics.umc_activity, 60u);
    EXPECT_EQ(cache_sample1.metrics.mm_activity, 45u);
    EXPECT_EQ(cache_sample1.metrics.hotspot_temperature, 85);
    EXPECT_EQ(cache_sample1.metrics.edge_temperature, 72);
    EXPECT_EQ(cache_sample1.metrics.xgmi.link.width, 16u);
    EXPECT_EQ(cache_sample1.metrics.xgmi.link.speed, 32000u);
    EXPECT_EQ(cache_sample1.metrics.xgmi.data_acc.read[0], 300000u);
    EXPECT_EQ(cache_sample1.metrics.xgmi.data_acc.write[0], 400000u);
    EXPECT_EQ(cache_sample1.metrics.pcie.link.width, 8u);
    EXPECT_EQ(cache_sample1.metrics.pcie.link.speed, 8000u);
    EXPECT_EQ(cache_sample1.metrics.pcie.bandwidth.acc, 800000u);
    EXPECT_EQ(cache_sample1.metrics.pcie.bandwidth.inst, 20000u);
    EXPECT_EQ(cache_sample1.metrics.xcp_stats[0].vcn_busy[0], 70u);
    EXPECT_EQ(cache_sample1.metrics.xcp_stats[0].jpeg_busy[0], 55u);

    EXPECT_EQ(cache_sample0.enabled.value, mock_settings_policy::s_enabled_metrics.value);
    EXPECT_EQ(cache_sample1.enabled.value, mock_settings_policy::s_enabled_metrics.value);
}

}  // namespace testing
}  // namespace amd_smi
}  // namespace rocprofsys

#endif  // ROCPROFSYS_USE_ROCM > 0
