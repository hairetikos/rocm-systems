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

#include "core/config.hpp"
#include "core/perfetto.hpp"
#include "library/amd_smi/common.hpp"
#include "library/thread_info.hpp"

#include <mutex>
#include <timemory/units.hpp>

#include <limits>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace rocprofsys
{
namespace amd_smi
{

#if ROCPROFSYS_USE_ROCM > 0

namespace
{

struct track_description
{
    const char*         track_name;
    const char*         units;
    std::vector<size_t> track_indexes;
};

const auto GFX_BUSY_VALUE = enabled_metric{ .bits = { .gfx_activity = 1 } }.value;
const auto UMC_BUSY_VALUE = enabled_metric{ .bits = { .umc_activity = 1 } }.value;
const auto MM_BUSY_VALUE  = enabled_metric{ .bits = { .mm_activity = 1 } }.value;
const auto TEMPERATURE_VALUE =
    enabled_metric{ .bits = { .hotspot_temperature = 1, .edge_temperature = 1 } }.value;
const auto CURRENT_POWER_VALUE =
    enabled_metric{ .bits = { .current_socket_power = 1, .average_socket_power = 1 } }.value;
const auto MEMORY_USAGE_VALUE  = enabled_metric{ .bits = { .memory_usage = 1 } }.value;
const auto VCN_ACTIVITY_VALUE  = enabled_metric{ .bits = { .vcn_activity = 1 } }.value;
const auto JPEG_ACTIVITY_VALUE = enabled_metric{ .bits = { .jpeg_activity = 1 } }.value;
const auto XGMI_VALUE          = enabled_metric{ .bits = { .xgmi = 1 } }.value;
const auto PCIE_VALUE          = enabled_metric{ .bits = { .pcie = 1 } }.value;

inline std::unordered_map<uint32_t, track_description>&
get_perfetto_tracks()
{
    static std::unordered_map<uint32_t, track_description> tracks{
        { GFX_BUSY_VALUE, { "GFX Busy", "%", {} } },
        { UMC_BUSY_VALUE, { "UMC Busy", "%", {} } },
        { MM_BUSY_VALUE, { "MM Busy", "%", {} } },
        { TEMPERATURE_VALUE, { "Temperature", "deg C", {} } },
        { CURRENT_POWER_VALUE, { "Current Power", "watts", {} } },
        { MEMORY_USAGE_VALUE, { "Memory Usage", "megabytes", {} } },
        { VCN_ACTIVITY_VALUE, { "VCN Activity", "%", {} } },
        { JPEG_ACTIVITY_VALUE, { "JPEG Activity", "%", {} } },
    };
    return tracks;
}

struct xgmi_track_set
{
    std::vector<size_t> link_width;
    std::vector<size_t> link_speed;
    std::vector<size_t> read_data;
    std::vector<size_t> write_data;
};

struct pcie_track_set
{
    std::vector<size_t> link_width;
    std::vector<size_t> link_speed;
    std::vector<size_t> bandwidth_acc;
    std::vector<size_t> bandwidth_inst;
};

inline std::map<size_t, xgmi_track_set>&
get_xgmi_tracks()
{
    static std::map<size_t, xgmi_track_set> tracks;
    return tracks;
}

inline std::map<size_t, pcie_track_set>&
get_pcie_tracks()
{
    static std::map<size_t, pcie_track_set> tracks;
    return tracks;
}

struct perfetto_amd_smi_sample
{
    size_t      timestamp;
    smi_metrics metrics;
};

inline std::map<size_t, std::unique_ptr<std::vector<perfetto_amd_smi_sample>>>&
get_perfetto_bundle()
{
    static std::map<size_t, std::unique_ptr<std::vector<perfetto_amd_smi_sample>>> bundle;
    return bundle;
}

}  // namespace

struct perfetto_policy
{
    using counter_track = perfetto_counter_track<smi_metrics>;

    template <typename ProcessorVector>
    static void init_storage(const ProcessorVector& processors)
    {
        for(const auto& processor : processors)
        {
            get_perfetto_bundle().insert(
                { processor->get_index(),
                  std::make_unique<std::vector<perfetto_amd_smi_sample>>() });
        }
    }

    static void setup_counter_tracks(size_t                device_index,
                                     const enabled_metric& enabled_metrics)
    {
        auto addendum = [&](const char* name) {
            return JOIN(" ", "GPU", name, JOIN("", '[', device_index, ']'), "(S)");
        };

        auto addendum_blk = [&](std::size_t i, const char* metric,
                                std::size_t xcp_idx = SIZE_MAX) {
            if(xcp_idx != SIZE_MAX)
            {
                return JOIN(" ", "GPU", JOIN("", '[', device_index, ']'), metric,
                            JOIN("", "XCP_", xcp_idx, ": [", (i < 10 ? "0" : ""), i, ']'),
                            "(S)");
            }
            return JOIN(" ", "GPU", JOIN("", '[', device_index, ']'), metric,
                        JOIN("", "[", (i < 10 ? "0" : ""), i, ']'), "(S)");
        };

        auto& tracks = get_perfetto_tracks();

        for(auto& [num, description] : tracks)
        {
            auto enabled_metric = num & enabled_metrics.value;
            if(enabled_metric == 0)
            {
                continue;
            }

            const auto process_xcp_array = [&](track_description& desc, size_t array_size,
                                               size_t xcp_id) {
                for(std::size_t i = 0; i < array_size; ++i)
                {
                    const auto track_id = counter_track::emplace(
                        device_index, addendum_blk(i, desc.track_name, xcp_id),
                        desc.units);
                    desc.track_indexes.emplace_back(track_id);
                }
            };

            if(enabled_metric == VCN_ACTIVITY_VALUE ||
               enabled_metric == JPEG_ACTIVITY_VALUE)
            {
                for(std::size_t xcp = 0; xcp < AMDSMI_MAX_NUM_XCP; ++xcp)
                {
                    process_xcp_array(description,
                                      enabled_metric == VCN_ACTIVITY_VALUE
                                          ? AMDSMI_MAX_NUM_VCN
                                          : ROCPROFSYS_MAX_NUM_JPEG_ENGINES,
                                      xcp);
                }
            }
            else
            {
                description.track_indexes.emplace_back(counter_track::emplace(
                    device_index, addendum(description.track_name), description.units));
            }
        }

        if(enabled_metrics.bits.xgmi)
        {
            auto& xgmi_tracks = get_xgmi_tracks()[device_index];

            xgmi_tracks.link_width.emplace_back(counter_track::emplace(
                device_index, addendum("XGMI Link Width"), "lanes"));
            xgmi_tracks.link_speed.emplace_back(counter_track::emplace(
                device_index, addendum("XGMI Link Speed"), "Mbps"));

            for(std::size_t link = 0; link < AMDSMI_MAX_NUM_XGMI_LINKS; ++link)
            {
                xgmi_tracks.read_data.emplace_back(counter_track::emplace(
                    device_index, addendum_blk(link, "XGMI Read Data"), "KB"));
                xgmi_tracks.write_data.emplace_back(counter_track::emplace(
                    device_index, addendum_blk(link, "XGMI Write Data"), "KB"));
            }
        }

        if(enabled_metrics.bits.pcie)
        {
            auto& pcie_tracks = get_pcie_tracks()[device_index];

            pcie_tracks.link_width.emplace_back(counter_track::emplace(
                device_index, addendum("PCIe Link Width"), "lanes"));
            pcie_tracks.link_speed.emplace_back(counter_track::emplace(
                device_index, addendum("PCIe Link Speed"), "MT/s"));
            pcie_tracks.bandwidth_acc.emplace_back(counter_track::emplace(
                device_index, addendum("PCIe Bandwidth Acc"), "bytes"));
            pcie_tracks.bandwidth_inst.emplace_back(counter_track::emplace(
                device_index, addendum("PCIe Bandwidth Inst"), "bytes/s"));
        }
    }

    static void store_sample(size_t device_index, const smi_metrics& metrics,
                             unsigned long timestamp)
    {
        get_perfetto_bundle()[device_index]->emplace_back(
            perfetto_amd_smi_sample{ timestamp, metrics });
    }

    template <typename ProcessorVector>
    static void post_process(const ProcessorVector& processors,
                             enabled_metric         enabled_metrics)
    {
        for(const auto& processor : processors)
        {
            post_process_device(processor->get_index(), enabled_metrics,
                                processor->get_supported_metrics());
        }
    }

private:
    static void post_process_device(size_t device_index, enabled_metric enabled_metrics,
                                    enabled_metric supported_metrics)
    {
        auto& samples = *get_perfetto_bundle()[device_index];

        printf("Post-processing %zu amd-smi samples from device %zu\n", samples.size(),
               device_index);

        ROCPROFSYS_VERBOSE(1, "Post-processing %zu amd-smi samples from device %zu\n",
                           samples.size(), device_index);

        const auto& thread_info = thread_info::get(0, InternalTID);
        if(!thread_info)
        {
            return;
        }

        enabled_metric effective_metrics = {
            .value =
                static_cast<uint32_t>(enabled_metrics.value & supported_metrics.value)
        };

        if(effective_metrics.value == 0)
        {
            ROCPROFSYS_WARNING(0, "No enabled AMD SMI metrics for device %zu\n",
                               device_index);
            return;
        }

        auto& tracks = get_perfetto_tracks();

        for(const auto& sample : samples)
        {
            const auto ts = sample.timestamp;

            if(!thread_info->is_valid_time(ts))
            {
                ROCPROFSYS_WARNING(0, "Invalid timestamp %zu for amd-smi sample\n", ts);
                continue;
            }

            process_basic_metrics(device_index, ts, sample.metrics, effective_metrics,
                                  tracks);
            process_xcp_activity(device_index, ts, sample.metrics, effective_metrics,
                                 enabled_metrics, supported_metrics, tracks);
            process_xgmi_metrics(device_index, ts, sample.metrics, effective_metrics);
            process_pcie_metrics(device_index, ts, sample.metrics, effective_metrics);
        }
    }

private:
    static void process_basic_metrics(
        size_t device_index, size_t ts, const smi_metrics& metrics,
        const enabled_metric&                            effective_metrics,
        std::unordered_map<uint32_t, track_description>& tracks)
    {
        if(effective_metrics.bits.gfx_activity &&
           !tracks.at(GFX_BUSY_VALUE).track_indexes.empty())
        {
            TRACE_COUNTER("device_busy_gfx",
                          counter_track::at(device_index,
                                            tracks.at(GFX_BUSY_VALUE).track_indexes[0]),
                          ts, static_cast<double>(metrics.gfx_activity));
        }

        if(effective_metrics.bits.umc_activity &&
           !tracks.at(UMC_BUSY_VALUE).track_indexes.empty())
        {
            TRACE_COUNTER("device_busy_umc",
                          counter_track::at(device_index,
                                            tracks.at(UMC_BUSY_VALUE).track_indexes[0]),
                          ts, static_cast<double>(metrics.umc_activity));
        }

        if(effective_metrics.bits.mm_activity &&
           !tracks.at(MM_BUSY_VALUE).track_indexes.empty())
        {
            TRACE_COUNTER("device_busy_mm",
                          counter_track::at(device_index,
                                            tracks.at(MM_BUSY_VALUE).track_indexes[0]),
                          ts, static_cast<double>(metrics.mm_activity));
        }

        if((effective_metrics.bits.edge_temperature ||
            effective_metrics.bits.hotspot_temperature) &&
           !tracks.at(TEMPERATURE_VALUE).track_indexes.empty())
        {
            const double temp = effective_metrics.bits.hotspot_temperature
                                    ? metrics.hotspot_temperature
                                    : metrics.edge_temperature;
            TRACE_COUNTER(
                "device_temp",
                counter_track::at(device_index,
                                  tracks.at(TEMPERATURE_VALUE).track_indexes[0]),
                ts, temp);
        }

        if((effective_metrics.bits.average_socket_power ||
            effective_metrics.bits.current_socket_power) &&
           !tracks.at(CURRENT_POWER_VALUE).track_indexes.empty())
        {
            const double power = effective_metrics.bits.average_socket_power
                                     ? metrics.average_socket_power
                                     : metrics.current_socket_power;
            TRACE_COUNTER(
                "device_power",
                counter_track::at(device_index,
                                  tracks.at(CURRENT_POWER_VALUE).track_indexes[0]),
                ts, power);
        }

        if(effective_metrics.bits.memory_usage &&
           !tracks.at(MEMORY_USAGE_VALUE).track_indexes.empty())
        {
            const double usage =
                metrics.memory_usage / static_cast<double>(units::megabyte);
            TRACE_COUNTER(
                "device_memory_usage",
                counter_track::at(device_index,
                                  tracks.at(MEMORY_USAGE_VALUE).track_indexes[0]),
                ts, usage);
        }
    }

    static void process_xcp_activity(
        size_t device_index, size_t ts, const smi_metrics& metrics,
        const enabled_metric& effective_metrics, const enabled_metric& enabled_metrics,
        const enabled_metric&                            supported_metrics,
        std::unordered_map<uint32_t, track_description>& tracks)
    {
        if(effective_metrics.bits.vcn_activity &&
           !tracks.at(VCN_ACTIVITY_VALUE).track_indexes.empty())
        {
            size_t engine_id = 0;
            for(const auto& xcp_stats : metrics.xcp_stats)
            {
                for(const auto& vcn_val : xcp_stats.vcn_busy)
                {
                    if(vcn_val != std::numeric_limits<uint16_t>::max() &&
                       engine_id < tracks.at(VCN_ACTIVITY_VALUE).track_indexes.size())
                    {
                        TRACE_COUNTER(
                            "device_vcn_activity",
                            counter_track::at(
                                device_index,
                                tracks.at(VCN_ACTIVITY_VALUE).track_indexes[engine_id++]),
                            ts, vcn_val);
                    }
                }
            }
        }

        static std::once_flag once_flag;
        std::call_once(once_flag, [&]() {
            printf("JPEG activity: %d, enabled: %d, supported: %d\n",
                   effective_metrics.bits.jpeg_activity, enabled_metrics.bits.jpeg_activity,
                   supported_metrics.bits.jpeg_activity);
        });

        if(effective_metrics.bits.jpeg_activity &&
           !tracks.at(JPEG_ACTIVITY_VALUE).track_indexes.empty())
        {
            size_t engine_id = 0;
            for(const auto& xcp_stats : metrics.xcp_stats)
            {
                for(const auto& jpeg_val : xcp_stats.jpeg_busy)
                {
                    if(jpeg_val != std::numeric_limits<uint16_t>::max() &&
                       engine_id < tracks.at(JPEG_ACTIVITY_VALUE).track_indexes.size())
                    {
                        TRACE_COUNTER("device_jpeg_activity",
                                      counter_track::at(device_index,
                                                        tracks.at(JPEG_ACTIVITY_VALUE)
                                                            .track_indexes[engine_id++]),
                                      ts, jpeg_val);
                    }
                }
            }
        }
    }

    static void process_xgmi_metrics(size_t device_index, size_t ts,
                                     const smi_metrics&    metrics,
                                     const enabled_metric& effective_metrics)
    {
        if(!effective_metrics.bits.xgmi)
        {
            return;
        }

        auto xgmi_it = get_xgmi_tracks().find(device_index);
        if(xgmi_it == get_xgmi_tracks().end())
        {
            return;
        }

        const auto& xgmi_tracks = xgmi_it->second;

        if(!xgmi_tracks.link_width.empty() && metrics.xgmi.link.width != 0)
        {
            TRACE_COUNTER("device_xgmi_link_width",
                          counter_track::at(device_index, xgmi_tracks.link_width[0]), ts,
                          static_cast<double>(metrics.xgmi.link.width));
        }

        if(!xgmi_tracks.link_speed.empty() && metrics.xgmi.link.speed != 0)
        {
            TRACE_COUNTER("device_xgmi_link_speed",
                          counter_track::at(device_index, xgmi_tracks.link_speed[0]), ts,
                          static_cast<double>(metrics.xgmi.link.speed));
        }

        for(size_t link = 0;
            link < AMDSMI_MAX_NUM_XGMI_LINKS && link < xgmi_tracks.read_data.size();
            ++link)
        {
            if(metrics.xgmi.data_acc.read[link] != 0)
            {
                TRACE_COUNTER(
                    "device_xgmi_read_data",
                    counter_track::at(device_index, xgmi_tracks.read_data[link]), ts,
                    static_cast<double>(metrics.xgmi.data_acc.read[link]));
            }
        }

        for(size_t link = 0;
            link < AMDSMI_MAX_NUM_XGMI_LINKS && link < xgmi_tracks.write_data.size();
            ++link)
        {
            if(metrics.xgmi.data_acc.write[link] != 0)
            {
                TRACE_COUNTER(
                    "device_xgmi_write_data",
                    counter_track::at(device_index, xgmi_tracks.write_data[link]), ts,
                    static_cast<double>(metrics.xgmi.data_acc.write[link]));
            }
        }
    }

    static void process_pcie_metrics(size_t device_index, size_t ts,
                                     const smi_metrics&    metrics,
                                     const enabled_metric& effective_metrics)
    {
        if(!effective_metrics.bits.pcie)
        {
            return;
        }

        auto pcie_it = get_pcie_tracks().find(device_index);
        if(pcie_it == get_pcie_tracks().end())
        {
            return;
        }

        const auto& pcie_tracks = pcie_it->second;

        if(!pcie_tracks.link_width.empty() && metrics.pcie.link.width != 0)
        {
            TRACE_COUNTER("device_pcie_link_width",
                          counter_track::at(device_index, pcie_tracks.link_width[0]), ts,
                          static_cast<double>(metrics.pcie.link.width));
        }

        if(!pcie_tracks.link_speed.empty() && metrics.pcie.link.speed != 0)
        {
            TRACE_COUNTER("device_pcie_link_speed",
                          counter_track::at(device_index, pcie_tracks.link_speed[0]), ts,
                          static_cast<double>(metrics.pcie.link.speed));
        }

        if(!pcie_tracks.bandwidth_acc.empty() && metrics.pcie.bandwidth.acc != 0)
        {
            TRACE_COUNTER("device_pcie_bandwidth_acc",
                          counter_track::at(device_index, pcie_tracks.bandwidth_acc[0]),
                          ts, static_cast<double>(metrics.pcie.bandwidth.acc));
        }

        if(!pcie_tracks.bandwidth_inst.empty() && metrics.pcie.bandwidth.inst != 0)
        {
            TRACE_COUNTER("device_pcie_bandwidth_inst",
                          counter_track::at(device_index, pcie_tracks.bandwidth_inst[0]),
                          ts, static_cast<double>(metrics.pcie.bandwidth.inst));
        }
    }
};

#endif  // ROCPROFSYS_USE_ROCM > 0

}  // namespace amd_smi
}  // namespace rocprofsys
