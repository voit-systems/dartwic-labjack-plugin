# DARTWIC LabJack T7 Plugin

This plugin adds LabJack T7 support to DARTWIC. It provides a module instance for connecting to a LabJack device, a periodic digital write task, and a worker stream task for publishing LabJack samples into RAPID channels.

## What It Provides

- `labjack_t7` module instances for LabJack T7 devices.
- `labjack.digital_write` periodic tasks for writing RAPID channel values to LabJack digital outputs.
- `labjack.stream` worker tasks for streaming analog or digital LabJack registers into RAPID channels.
- A DARTWIC interface plugin for configuring the module and task mappings.
- Stream health telemetry, including LabJack connection state, last read age, and hardware backlog values.

## Requirements

- DARTWIC with engine plugin API version 2 and interface plugin API version 1.
- LabJack LJM installed on the host machine.
- A LabJack T7 reachable through the configured LJM connection settings.

The plugin links against the LabJack LJM driver library included under `engine/libs/LabJack/Drivers`.

## Module Configuration

Create a module instance with module type `labjack_t7`.

Default parameters:

```json
{
  "device_type": "T7",
  "connection_type": "ANY",
  "identifier": "ANY"
}
```

Fields:

- `device_type`: The LJM device type. Use `T7` for a physical LabJack T7.
- `connection_type`: The LJM connection type, such as `ANY`, `USB`, `ETHERNET`, or `WIFI`.
- `identifier`: The LJM identifier, such as `ANY`, a serial number, an IP address, or `LJM_DEMO_MODE`.

The module publishes:

- `<module_instance>/device_connected`: `1` when connected, `0` when disconnected.

The module connection loop keeps trying to connect once per second when disconnected and verifies an active connection once per second when connected.

## Digital Write Task

Task type:

```text
labjack.digital_write
```

This is a periodic task. On each task run, it reads the configured RAPID source channels and writes their values to LabJack digital outputs.

Arguments:

```json
{
  "module_instance_name": "my_labjack",
  "mappings": [
    {
      "register": 0,
      "channel": "portal/output_enable"
    }
  ]
}
```

Mapping fields:

- `register`: The digital register number. Register `0` maps to `DIO0`, register `1` maps to `DIO1`, and so on.
- `channel`: The RAPID channel used as the desired output value.

Values are written as:

- `0` when the source channel value is `0`.
- `1` when the source channel value is any non-zero value.

For each mapped source channel, the task also publishes a readback state channel:

```text
<source_channel>_state
```

For example, `portal/output_enable` creates:

```text
portal/output_enable_state
```

State channels are configured with:

- `stale_timeout`: `1` second.
- `control_policy`: `observe_only`.

## Stream Task

Task type:

```text
labjack.stream
```

This is a worker task. It starts a LabJack hardware stream and writes every sample into the configured RAPID destination channels.

Arguments:

```json
{
  "module_instance_name": "my_labjack",
  "target_scan_rate": 1000,
  "scans_per_read": 10,
  "mappings": [
    {
      "channel_type": "analog",
      "register": 0,
      "channel": "portal/ain0"
    },
    {
      "channel_type": "digital",
      "register": 1,
      "channel": "portal/dio1"
    }
  ]
}
```

Stream fields:

- `target_scan_rate`: Requested LabJack stream scan rate in scans per second.
- `scans_per_read`: Number of scans requested from LJM on each read.
- `mappings`: Registers to stream into RAPID channels.

Mapping fields:

- `channel_type`: `analog` maps to `AIN<n>`. `digital` maps to `DIO<n>`.
- `register`: The LabJack register number.
- `channel`: The RAPID destination channel.

Only one stream task can own the LabJack hardware stream at a time for a module instance.

## Stream Channel Behavior

When the stream task starts, each mapped destination channel is configured with:

- `record_mode`: `every_value`.
- `control_policy`: `observe_only`.
- `control_owner`: `task:<portal>/<task_name>`.
- `active_controller`: `task:<portal>/<task_name>`.
- `stale_timeout`: `max(1 second, 2 / (target_scan_rate / scans_per_read))`.

That stale timeout means the channel will not go stale faster than one second, but slower stream read rates can receive a larger timeout automatically.

The stream task writes samples through RAPID bulk value inserts. Sample timestamps are calculated from the stream start time, scan number, and actual scan rate.

## Stream Reconnect Behavior

The stream worker is designed to stay alive while the task is on.

If the LabJack disconnects during streaming, the worker:

- Marks `<module_instance>/device_connected` as `0`.
- Closes the stale LJM handle.
- Keeps updating stream diagnostics.
- Attempts to reconnect.
- Restarts the hardware stream after reconnecting.

If stream start fails because LJM reports that the stream is already active, the worker stops the existing hardware stream and retries the stream start.

Harmless stop errors such as `STREAM_NOT_RUNNING` are ignored so task startup and shutdown do not fail just because the hardware stream was already stopped.

## Stream Diagnostics

The stream task publishes diagnostic channels under the task portal using the task name as the prefix.

For a task named `stream`, diagnostics look like:

```text
<task_portal>/stream_stream_target_scan_rate
<task_portal>/stream_stream_scans_per_read
<task_portal>/stream_stream_actual_scan_rate
<task_portal>/stream_stream_expected_read_rate
<task_portal>/stream_stream_worker_read_rate
<task_portal>/stream_stream_last_read_ms
<task_portal>/stream_stream_device_scan_backlog
<task_portal>/stream_stream_ljm_scan_backlog
```

Diagnostics:

- `target_scan_rate`: Requested scan rate.
- `scans_per_read`: Configured scans per LJM read.
- `actual_scan_rate`: Scan rate accepted by LJM after stream start.
- `expected_read_rate`: `actual_scan_rate / scans_per_read`.
- `worker_read_rate`: Reads per second completed by the worker.
- `last_read_ms`: Time in milliseconds since the previous successful stream read. This resets to `0` when the stream task stops.
- `device_scan_backlog`: Device scan backlog reported by `LJM_eStreamRead`.
- `ljm_scan_backlog`: LJM scan backlog reported by `LJM_eStreamRead`.

Diagnostic channels are configured as observe-only and owned by the stream task.

## Task Card UI

The interface plugin adds configuration and status UI for LabJack tasks.

The stream task card shows:

- Module instance name.
- `CONNECTED` or `DISCONNECTED` badge from `<module_instance>/device_connected`.
- Mapping count.
- Last read time in milliseconds.
- Device scan backlog.
- LJM scan backlog.

The stream metrics panel shows:

- Target scan rate.
- Scans per read.
- Actual scan rate.

The digital write task editor shows only the source mapping rows. Generated `_state` channels are created automatically by the engine task and are not edited from the mapping UI.

## Demo Mode

Set `identifier`, `device_type`, or `connection_type` to `LJM_DEMO_MODE` to run against LJM demo mode.

In demo mode:

- Stream values are generated randomly.
- Digital write state channels mirror the commanded source values.

## Building

Build the interface plugin:

```powershell
npm.cmd run build
```

Build the engine plugin with CMake:

```powershell
cmake --build cmake-build-debug
```

If the configured CMake generator is `nmake`, run the build from a Visual Studio Developer PowerShell or regenerate the build directory with an available generator.

## Development Notes

- `plugin.json` declares plugin id `labjack_t7`.
- Engine source lives in `engine/src`.
- Interface source lives in `interface/src`.
- Built plugin artifacts are written under `plugin/`.
- `build-configuration.json` can be used to copy built plugin artifacts into a DARTWIC engine plugin directory.
