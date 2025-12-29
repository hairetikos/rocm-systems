# AMD SMI Subsystem Architecture

This document describes the architecture and design of the AMD SMI (System Management Interface) subsystem used for GPU metrics collection in rocprofiler-systems.

## Overview

The AMD SMI subsystem provides hardware-level GPU metrics collection including:
- GPU utilization (GFX, UMC, MM busy percentages)
- Power consumption (current and average socket power)
- Temperature readings (hotspot and edge)
- Memory usage (VRAM utilization)
- VCN/JPEG engine activity
- PCIe and XGMI link statistics

## Architecture

The subsystem follows a **policy-based design** with **dependency injection** to achieve:
- High testability through mockable interfaces
- Clean separation of concerns
- Flexible output backends (Perfetto, RocPD)
- Runtime configurability

### Component Diagram

```
┌─────────────────────────────────────────────────────────────────────────┐
│                           amd_smi_impl<Config>                          │
│                         (Main Implementation)                           │
├─────────────────────────────────────────────────────────────────────────┤
│  Template Parameters (Policies):                                        │
│  ┌──────────────────┐ ┌──────────────────┐ ┌──────────────────────────┐ │
│  │ SmiServiceFactory│ │   SettingsApi    │ │ PerfettoApi / RocpdApi   │ │
│  │ (Device Access)  │ │ (Configuration)  │ │   (Output Backends)      │ │
│  └────────┬─────────┘ └──────────────────┘ └──────────────────────────┘ │
└───────────┼─────────────────────────────────────────────────────────────┘
            │
            ▼
┌───────────────────────┐
│   service<Driver>     │
│  (Lifecycle Manager)  │
├───────────────────────┤
│ - init/shutdown       │
│ - get_processors()    │
│ - get_version()       │
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐     ┌────────────────────────┐
│  processor<Driver>    │────▶│    amd_smi_driver      │
│  (Metrics Collector)  │     │   (Hardware Access)    │
├───────────────────────┤     ├────────────────────────┤
│ - get_smi_metrics()   │     │ - amdsmi_* API calls   │
│ - get_supported_*()   │     │ - Error handling       │
│ - device index        │     └────────────────────────┘
└───────────────────────┘
```

## Components

### 1. `amd_smi_driver` (`amd_smi_driver.hpp`)

Low-level wrapper around the AMD SMI C API. Provides a mockable interface for hardware access.

```cpp
struct amd_smi_driver
{
    static amdsmi_status_t init(uint64_t init_flags = AMDSMI_INIT_AMD_GPUS);
    static amdsmi_status_t shutdown();
    static amdsmi_status_t get_activity(amdsmi_processor_handle, amdsmi_engine_usage_t*);
    static amdsmi_status_t get_power_info(amdsmi_processor_handle, amdsmi_power_info_t*);
    static amdsmi_status_t get_temperature_metric(...);
    static amdsmi_status_t get_memory_usage(...);
    static amdsmi_status_t get_metrics_info(...);
    // ... additional methods
};
```

**Key Features:**
- Static methods wrapping `amdsmi_*` C functions
- Handles API version differences (e.g., `amdsmi_get_power_info` signature changes)
- Factory pattern via `amd_smi_driver_factory`

### 2. `processor<Driver>` (`processor.hpp`)

Represents a single GPU processor and handles metrics collection.

```cpp
template <typename Driver>
class processor
{
public:
    processor(std::shared_ptr<Driver> driver, amdsmi_processor_handle handle,
              processor_type_t type, size_t logical_index);
    
    smi_metrics    get_smi_metrics() const;      // Collect all metrics
    enabled_metric get_supported_metrics() const; // Query supported metrics
    size_t         get_index() const;             // Logical device index
};
```

**Key Features:**
- Automatic detection of supported metrics at construction
- Graceful handling of unsupported metrics
- Aggregates multiple metric categories into single `smi_metrics` struct

### 3. `service<DriverFactory>` (`service.hpp`)

Service layer managing SMI lifecycle and processor discovery.

```cpp
template <typename DriverFactory>
class service
{
public:
    service();  // Initializes AMD SMI
    
    const version& get_version() const;
    processor_vector_t get_processors(const filter_func_t& filter = nullptr);
    void shutdown();
};
```

**Key Features:**
- RAII-style initialization
- Processor enumeration across all sockets
- Optional filtering callback for device selection

### 4. `amd_smi_impl<Config>` (`amd_smi_impl.hpp`)

Main implementation using policy-based design.

```cpp
template <typename Config>
struct amd_smi_impl
{
    using SmiServiceFactory = typename Config::SmiServiceFactory;
    using SettingsApi       = typename Config::SettingsApi;
    using PerfettoApi       = typename Config::PerfettoApi;
    using CacheApi          = typename Config::RocpdApi;
    
    void setup();                                    // Initialize subsystem
    void config();                                   // Configure output backends
    void sample(const get_timestamp_t& get_ts);     // Collect metrics sample
    void post_process();                            // Finalize output
    void shutdown();                                // Cleanup
};
```

**Key Features:**
- Compile-time policy injection
- Decoupled from specific output backends
- Supports device filtering via settings policy

### 5. Policy Classes

#### `settings_policy` (`settings.hpp`)

Handles runtime configuration from environment variables.

```cpp
struct settings_policy
{
    static device_filter  get_device_filter();   // ROCPROFSYS_SAMPLING_GPUS
    static enabled_metric get_enabled_metrics(); // ROCPROFSYS_AMD_SMI_METRICS
};
```

**Configuration Options:**
- `ROCPROFSYS_SAMPLING_GPUS`: Device selection (`all`, `none`, `0,1,2`, `0-3`)
- `ROCPROFSYS_AMD_SMI_METRICS`: Metric selection (`all`, `temp`, `power`, `busy`, etc.)

#### `perfetto_policy` (`perfetto_policy.hpp`)

Handles Perfetto trace output.

```cpp
struct perfetto_policy
{
    static void init_storage(size_t device_index);
    static void setup_counter_tracks(size_t device_index, const enabled_metric&);
    static void store_sample(size_t device_index, const smi_metrics&, unsigned long ts);
    static void post_process(size_t device_index, enabled_metric enabled, enabled_metric supported);
};
```

#### `rocpd_policy` (`rocpd_policy.hpp`)

Handles RocPD (ROCm Profiler Database) output.

```cpp
struct rocpd_policy
{
    static void initialize_category_metadata();
    static void initialize_smi_tracks_metadata(size_t gpu_id);
    static void initialize_smi_pmc_metadata(size_t gpu_id);
    static void store_sample(size_t device_id, const enabled_metric& supported,
                            const enabled_metric& enabled, const smi_metrics&, unsigned long ts);
};
```

## Data Types

### `enabled_metric` (`common.hpp`)

Bitfield union for metric selection and capability tracking.

```cpp
union enabled_metric
{
    struct {
        uint32_t current_socket_power : 1;
        uint32_t average_socket_power : 1;
        uint32_t memory_usage         : 1;
        uint32_t hotspot_temperature  : 1;
        uint32_t edge_temperature     : 1;
        uint32_t gfx_activity         : 1;
        uint32_t umc_activity         : 1;
        uint32_t mm_activity          : 1;
        uint32_t vcn_activity         : 1;
        uint32_t jpeg_activity        : 1;
        uint32_t xgmi                 : 1;
        uint32_t pcie                 : 1;
    } bits;
    uint32_t value = 0;
};
```

### `smi_metrics` (`common.hpp`)

Container for all collected metrics.

```cpp
struct smi_metrics
{
    uint32_t current_socket_power;
    uint32_t average_socket_power;
    uint64_t memory_usage;
    int64_t  hotspot_temperature;
    int64_t  edge_temperature;
    uint32_t gfx_activity;
    uint32_t umc_activity;
    uint32_t mm_activity;
    // VCN/JPEG per-XCP arrays
    std::array<xcp_metrics, AMDSMI_MAX_NUM_XCP> xcp_stats;
    // XGMI/PCIe link statistics
    // ...
};
```

## Data Flow

```
┌──────────────┐    ┌──────────────┐    ┌──────────────────────┐
│   Hardware   │───▶│   Driver     │───▶│     Processor        │
│  (AMD GPUs)  │    │ (amdsmi API) │    │ (metrics collection) │
└──────────────┘    └──────────────┘    └──────────┬───────────┘
                                                   │
                                                   ▼
                                        ┌──────────────────────┐
                                        │    amd_smi_impl      │
                                        │   (orchestration)    │
                                        └──────────┬───────────┘
                                                   │
                         ┌─────────────────────────┼─────────────────────────┐
                         ▼                         ▼                         ▼
              ┌──────────────────┐     ┌──────────────────┐     ┌──────────────────┐
              │  perfetto_policy │     │   rocpd_policy   │     │ (future outputs) │
              │  (trace output)  │     │   (DB output)    │     │                  │
              └──────────────────┘     └──────────────────┘     └──────────────────┘
```

## Lifecycle

### Initialization Phase

```cpp
// 1. setup() - Initialize hardware and discover devices
amd_smi_impl<config>::setup();
  └── SmiServiceFactory::create_smi_service()
      └── Driver::init()
      └── Enumerate sockets and processors
      └── Apply device filter (SettingsApi::get_device_filter())
      └── PerfettoApi::init_storage() for each device

// 2. config() - Configure output backends
amd_smi_impl<config>::config();
  └── CacheApi::initialize_category_metadata()
  └── For each device:
      └── PerfettoApi::setup_counter_tracks()
      └── CacheApi::initialize_smi_tracks_metadata()
      └── CacheApi::initialize_smi_pmc_metadata()
```

### Sampling Phase

```cpp
// Called periodically during profiling
amd_smi_impl<config>::sample(get_timestamp);
  └── For each processor:
      └── processor::get_smi_metrics()
          └── collect_activity_metrics()
          └── collect_power_metrics()
          └── collect_temperature_metrics()
          └── collect_memory_metrics()
          └── collect_gpu_metrics()
      └── CacheApi::store_sample()
      └── PerfettoApi::store_sample()
```

### Finalization Phase

```cpp
// 1. post_process() - Finalize output data
amd_smi_impl<config>::post_process();
  └── For each processor:
      └── PerfettoApi::post_process()

// 2. shutdown() - Cleanup
amd_smi_impl<config>::shutdown();
  └── service::shutdown()
      └── Driver::shutdown()
```

## Testing

The architecture enables comprehensive unit testing through dependency injection.

### Mock Classes

```cpp
// Mock driver for unit tests
class mock_driver
{
public:
    MOCK_METHOD(amdsmi_status_t, init, (uint64_t));
    MOCK_METHOD(amdsmi_status_t, get_activity, (amdsmi_processor_handle, amdsmi_engine_usage_t*));
    // ... other mocked methods
};

// Mock policies for testing amd_smi_impl
struct mock_settings_policy { /* controllable settings */ };
struct mock_perfetto_policy { /* verify output calls */ };
struct mock_rocpd_policy    { /* verify output calls */ };
```

### Test Configuration

```cpp
struct test_config
{
    using SmiServiceFactory = mock_service_factory;
    using SettingsApi       = mock_settings_policy;
    using PerfettoApi       = mock_perfetto_policy;
    using RocpdApi          = mock_rocpd_policy;
};

// Use in tests
amd_smi_impl<test_config> impl;
impl.setup();
impl.sample([]() { return test_timestamp; });
// Verify mock expectations
```

### Running Tests

```bash
# Build with testing enabled
cmake -DROCPROFSYS_BUILD_TESTING=ON ...
make

# Run AMD SMI tests
ctest -R amd_smi
```

## Configuration

### Environment Variables

| Variable | Description | Values | Default |
|----------|-------------|--------|---------|
| `ROCPROFSYS_SAMPLING_GPUS` | Device selection | `all`, `none`, `0,1,2`, `0-3` | `all` |
| `ROCPROFSYS_AMD_SMI_METRICS` | Metrics to collect | `all`, `none`, comma-separated list | `all` |

### Metric Keywords

| Keyword | Metrics Enabled |
|---------|----------------|
| `temp` | hotspot_temperature, edge_temperature |
| `power` | current_socket_power, average_socket_power |
| `busy` | gfx_activity, umc_activity, mm_activity |
| `mem_usage` | memory_usage |
| `vcn_activity` | vcn_activity |
| `jpeg_activity` | jpeg_activity |
| `xgmi` | XGMI link statistics |
| `pcie` | PCIe link statistics |

**Example:**
```bash
export ROCPROFSYS_AMD_SMI_METRICS="temp,power,busy"
export ROCPROFSYS_SAMPLING_GPUS="0,1"
```

## Extending the System

### Adding a New Output Backend

1. Create a new policy class implementing the required interface:

```cpp
struct my_output_policy
{
    static void init_storage(size_t device_index);
    static void setup_counter_tracks(size_t device_index, const enabled_metric&);
    static void store_sample(size_t device_index, const smi_metrics&, unsigned long ts);
    static void post_process(size_t device_index, enabled_metric enabled, enabled_metric supported);
};
```

2. Create a configuration struct using your policy:

```cpp
struct my_config
{
    using SmiServiceFactory = smi_service_factory<amd_smi_driver_factory>;
    using SettingsApi       = settings_policy;
    using PerfettoApi       = my_output_policy;  // or add as additional policy
    using RocpdApi          = rocpd_policy;
};
```

### Adding New Metrics

1. Add bitfield to `enabled_metric` in `common.hpp`
2. Add field to `smi_metrics` struct
3. Implement collection in `processor::collect_*_metrics()`
4. Update `processor::initialize_supported_metrics()`
5. Update output policies to handle new metric
6. Add configuration keyword in `settings_policy::parse_enabled_metrics()`

## File Structure

```
source/lib/rocprof-sys/library/amd_smi/
├── CMakeLists.txt          # Build configuration
├── README.md               # This documentation
├── common.hpp              # Shared types and utilities
├── amd_smi_driver.hpp      # Hardware abstraction layer
├── processor.hpp           # Per-device metrics collection
├── service.hpp             # Lifecycle management
├── settings.hpp            # Configuration policy
├── perfetto_policy.hpp     # Perfetto output policy
├── rocpd_policy.hpp        # RocPD output policy
├── amd_smi_impl.hpp        # Main template implementation
├── amd_smi_sample.hpp      # Serialization for trace cache
└── tests/
    ├── CMakeLists.txt
    ├── mock_driver.hpp         # Mock driver for testing
    ├── tests_processor.cpp     # Processor unit tests
    ├── tests_service.cpp       # Service unit tests
    └── tests_amd_smi_impl.cpp  # Integration tests
```

## Design Rationale

### Why Policy-Based Design?

1. **Testability**: Policies can be mocked for unit testing without hardware
2. **Flexibility**: Output backends can be swapped at compile time
3. **Separation of Concerns**: Each policy handles one specific responsibility
4. **Zero Runtime Overhead**: Template-based design eliminates virtual function calls

### Why Dependency Injection?

1. **Loose Coupling**: Components don't depend on concrete implementations
2. **Mockability**: Easy to substitute test doubles
3. **Configurability**: Different configurations for different use cases

### Why Template-Based?

1. **Performance**: No virtual dispatch overhead in hot paths
2. **Type Safety**: Compile-time checking of policy interfaces
3. **Optimization**: Compiler can inline policy method calls
