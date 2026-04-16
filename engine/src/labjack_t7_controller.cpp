#include "labjack_t7_controller.h"

#include "labjack_t7_module.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iostream>
#include <random>
#include <sstream>
#include <thread>

using DARTWIC::API::ChannelField;
using DARTWIC::API::ChannelValue;
using DARTWIC::API::ControlPolicy;
using DARTWIC::API::RecordMode;

namespace {
    constexpr int LJM_DEVICE_STREAM_IS_ACTIVE = 2605;
    constexpr int LJM_DEVICE_STREAM_NOT_RUNNING = 2620;

    std::string taskKey(DARTWIC::API::TaskRuntime& task_runtime) {
        return task_runtime.getPortalName() + "/" + task_runtime.getTaskName();
    }

    std::string stateChannelName(const std::string& channel) {
        return channel + "_state";
    }

    std::string trimAndUpper(std::string value) {
        value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), value.end());
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return value;
    }

    bool isDemoModeToken(const std::string& value) {
        const auto normalized = trimAndUpper(value);
        return normalized == LJM_DEMO_MODE || normalized == "LJM_DEMO_MODE";
    }

    bool isStreamAlreadyStartedError(int error_number) {
        if (error_number == LJM_DEVICE_STREAM_IS_ACTIVE) {
            return true;
        }

        char error_string[LJM_MAX_NAME_SIZE] = "LJM error not found.";
        LJM_ErrorToString(error_number, error_string);
        const auto normalized = trimAndUpper(error_string);
        return normalized.find("STREAM") != std::string::npos &&
            (
                normalized.find("STREAM_IS_ACTIVE") != std::string::npos ||
                (
                    normalized.find("ALREADY") != std::string::npos &&
                    (normalized.find("START") != std::string::npos || normalized.find("RUN") != std::string::npos)
                )
            );
    }

    bool isHarmlessStreamStopError(int error_number) {
        return error_number == LJME_NOERROR ||
            error_number == LJME_STREAM_NOT_INITIALIZED ||
            error_number == LJME_STREAM_NOT_RUNNING ||
            error_number == LJM_DEVICE_STREAM_NOT_RUNNING;
    }
}

LabJackT7Controller::LabJackT7Controller(
    LabJackT7Module* module,
    std::string instance_name,
    std::string device_type,
    std::string connection_type,
    std::string identifier
) : module_(module),
    instance_name_(std::move(instance_name)),
    device_type_(std::move(device_type)),
    connection_type_(std::move(connection_type)),
    identifier_(std::move(identifier)),
    connection_loop_name_("labjack_t7_connection_" + instance_name_) {
    module_->dartwic->onStart(connection_loop_name_, [this]() { connectionLoopStart(); });
    module_->dartwic->onLoop(connection_loop_name_, [this]() { connectionLoop(); });
    module_->dartwic->onEnd(connection_loop_name_, [this]() { connectionLoopEnd(); });
}

LabJackT7Controller::~LabJackT7Controller() {
    stopStream();
    module_->dartwic->removeLoop(connection_loop_name_);
}

bool LabJackT7Controller::isConnected() const {
    return connected_.load();
}

double LabJackT7Controller::query(const std::string& channel, double default_value) const {
    return module_->dartwic->queryChannelField(instance_name_, channel, ChannelField::VALUE, ChannelValue{default_value});
}

double LabJackT7Controller::query(const RapidChannel& channel, double default_value) const {
    return module_->dartwic->queryChannelField(channel.portal, channel.channel, ChannelField::VALUE, ChannelValue{default_value});
}

void LabJackT7Controller::upsert(const std::string& channel, ChannelValue value) const {
    module_->dartwic->upsertChannelField(instance_name_, channel, ChannelField::VALUE, std::move(value));
}

void LabJackT7Controller::upsert(const RapidChannel& channel, ChannelValue value) const {
    module_->dartwic->upsertChannelField(channel.portal, channel.channel, ChannelField::VALUE, std::move(value));
}

void LabJackT7Controller::upsertBulk(const RapidChannel& channel, const std::vector<std::pair<double, uint64_t>>& data) const {
    module_->dartwic->upsertChannelValueBulk(channel.portal, channel.channel, data);
}

void LabJackT7Controller::consoleError(
    const std::string& title,
    const std::string& description,
    std::vector<std::string> channels,
    const std::string& resolution,
    int auto_ack
) const {
    module_->dartwic->consoleError(title, description, std::move(channels), resolution, auto_ack);
}

void LabJackT7Controller::configureStreamChannelFields(
    const std::vector<StreamMapping>& mappings,
    double stale_timeout_seconds,
    const std::string& controller
) const {
    for (const auto& mapping : mappings) {
        module_->dartwic->upsertChannelField(
            mapping.destination.portal,
            mapping.destination.channel,
            ChannelField::STALE_TIMEOUT,
            stale_timeout_seconds
        );
        module_->dartwic->upsertChannelField(
            mapping.destination.portal,
            mapping.destination.channel,
            ChannelField::RECORD_MODE,
            RecordMode::EveryValue
        );
        configureObserveOnlyChannel(mapping.destination.portal, mapping.destination.channel, controller);
    }
}

void LabJackT7Controller::configureObserveOnlyChannel(
    const std::string& portal,
    const std::string& channel,
    const std::string& controller
) const {
    module_->dartwic->upsertChannelField(portal, channel, ChannelField::CONTROL_POLICY, ControlPolicy::ObserveOnly);
    module_->dartwic->upsertChannelField(portal, channel, ChannelField::CONTROL_OWNER, controller);
    module_->dartwic->upsertChannelField(portal, channel, ChannelField::ACTIVE_CONTROLLER, controller);
}

void LabJackT7Controller::connectionLoopStart() {
    upsert("device_connected", 0.0);
}

void LabJackT7Controller::connectionLoop() {
    if (handle_ == -1) {
        const int error = connect();
        if (error != LJME_NOERROR) {
            handleError(error, "connect");
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return;
    }

    verifyConnection();
    std::this_thread::sleep_for(std::chrono::seconds(1));
}

void LabJackT7Controller::connectionLoopEnd() {
    stopStream();
    disconnect();
}

int LabJackT7Controller::connect() {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    demo_mode_ =
        isDemoModeToken(identifier_) ||
        isDemoModeToken(device_type_) ||
        isDemoModeToken(connection_type_);

    if (handle_ != -1) {
        connected_ = true;
        upsert("device_connected", 1.0);
        return LJME_NOERROR;
    }

    const std::string device_type = demo_mode_ ? "T7" : device_type_;
    const std::string connection_type = demo_mode_ ? "ANY" : connection_type_;
    const std::string identifier = demo_mode_ ? LJM_DEMO_MODE : identifier_;

    const int error = LJM_OpenS(device_type.c_str(), connection_type.c_str(), identifier.c_str(), &handle_);
    if (error != LJME_NOERROR) {
        handle_ = -1;
        connected_ = false;
        upsert("device_connected", 0.0);
        return error;
    }

    connected_ = true;
    upsert("device_connected", 1.0);
    return LJME_NOERROR;
}

void LabJackT7Controller::disconnect() {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    if (handle_ != -1) {
        LJM_Close(handle_);
        handle_ = -1;
    }
    connected_ = false;
    upsert("device_connected", 0.0);
}

void LabJackT7Controller::verifyConnection() {
    if (demo_mode_) {
        connected_ = true;
        upsert("device_connected", 1.0);
        return;
    }

    double value = 0.0;
    int error = LJME_NOERROR;
    {
        std::lock_guard<std::mutex> lock(handle_mutex_);
        if (handle_ == -1) {
            connected_ = false;
            upsert("device_connected", 0.0);
            return;
        }

        error = LJM_eReadName(handle_, "SERIAL_NUMBER", &value);
        if (error != LJME_NOERROR && handle_ != -1) {
            LJM_Close(handle_);
            handle_ = -1;
        }
    }

    if (error == LJME_NOERROR) {
        connected_ = true;
        upsert("device_connected", 1.0);
        return;
    }

    connected_ = false;
    upsert("device_connected", 0.0);
    handleError(error, "verify_connection");
}

void LabJackT7Controller::markDisconnectedFromStreamError() {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    if (handle_ != -1) {
        LJM_Close(handle_);
        handle_ = -1;
    }
    connected_ = false;
    upsert("device_connected", 0.0);
}

void LabJackT7Controller::handleError(int error_number, const std::string& operation) {
    char error_string[LJM_MAX_NAME_SIZE] = "LJM error not found.";
    LJM_ErrorToString(error_number, error_string);
    consoleError(
        "LabJack T7 Error [" + instance_name_ + "]",
        "LabJack operation failed.\n[Operation: " + operation + "]\n[LJM Error: " + std::string(error_string) + "]",
        {instance_name_ + "/device_connected.value"},
        "Check the LabJack connection, stream configuration, and device state.",
        (operation == "connect" || operation == "verify_connection") ? 5 : 0
    );
}

std::optional<LabJackT7Controller::RapidChannel> LabJackT7Controller::parseRapidChannel(const nlohmann::json& value) const {
    if (!value.is_string()) {
        return std::nullopt;
    }
    return splitRapidChannelPath(value.get<std::string>());
}

std::optional<LabJackT7Controller::RapidChannel> LabJackT7Controller::splitRapidChannelPath(const std::string& channel_path) const {
    const auto separator = channel_path.find('/');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= channel_path.size()) {
        return std::nullopt;
    }
    return RapidChannel{
        .portal = channel_path.substr(0, separator),
        .channel = channel_path.substr(separator + 1)
    };
}

std::string LabJackT7Controller::buildStreamLabJackName(const nlohmann::json& mapping) const {
    const std::string channel_type = mapping.value("channel_type", "analog");
    const int register_number = mapping.value("register", 0);
    return (channel_type == "digital" ? "DIO" : "AIN") + std::to_string(register_number);
}

std::string LabJackT7Controller::buildDigitalLabJackName(const nlohmann::json& mapping) const {
    return "DIO" + std::to_string(mapping.value("register", 0));
}

std::vector<LabJackT7Controller::StreamMapping> LabJackT7Controller::parseStreamMappings(const nlohmann::json& arguments) {
    std::vector<StreamMapping> resolved;
    if (!arguments.contains("mappings") || !arguments["mappings"].is_array()) {
        return resolved;
    }

    for (const auto& mapping : arguments["mappings"]) {
        if (!mapping.is_object() || !mapping.contains("channel")) {
            continue;
        }

        auto destination = parseRapidChannel(mapping["channel"]);
        if (!destination.has_value()) {
            continue;
        }

        const std::string labjack_name = buildStreamLabJackName(mapping);
        int address = 0;
        int type = 0;
        const int error = LJM_NameToAddress(labjack_name.c_str(), &address, &type);
        if (error != LJME_NOERROR) {
            handleError(error, "resolve_stream_channel:" + labjack_name);
            continue;
        }

        resolved.push_back(StreamMapping{
            .labjack_name = labjack_name,
            .destination = *destination,
            .address = address
        });
    }

    return resolved;
}

std::vector<LabJackT7Controller::DigitalWriteMapping> LabJackT7Controller::parseDigitalWriteMappings(const nlohmann::json& arguments) const {
    std::vector<DigitalWriteMapping> resolved;
    if (!arguments.contains("mappings") || !arguments["mappings"].is_array()) {
        return resolved;
    }

    for (const auto& mapping : arguments["mappings"]) {
        if (!mapping.is_object() || !mapping.contains("channel")) {
            continue;
        }

        auto source = parseRapidChannel(mapping["channel"]);
        if (!source.has_value()) {
            continue;
        }

        resolved.push_back(DigitalWriteMapping{
            .labjack_name = buildDigitalLabJackName(mapping),
            .source = *source,
            .state = RapidChannel{
                .portal = source->portal,
                .channel = stateChannelName(source->channel)
            }
        });
    }

    return resolved;
}

void LabJackT7Controller::publishTaskDiagnostic(
    DARTWIC::API::TaskRuntime& task_runtime,
    const std::string& suffix,
    ChannelValue value
) const {
    const std::string channel = task_runtime.getTaskName() + suffix;
    module_->dartwic->upsertChannelField(
        task_runtime.getPortalName(),
        channel,
        ChannelField::VALUE,
        std::move(value)
    );
    configureObserveOnlyChannel(
        task_runtime.getPortalName(),
        channel,
        "task:" + task_runtime.getPortalName() + "/" + task_runtime.getTaskName()
    );
}

bool LabJackT7Controller::tryAcquireStream(const std::string& task_key) {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (!active_stream_task_key_.empty()) {
        return false;
    }

    active_stream_task_key_ = task_key;
    return true;
}

void LabJackT7Controller::releaseStream(const std::string& task_key) {
    std::lock_guard<std::mutex> lock(stream_mutex_);
    if (active_stream_task_key_ != task_key) {
        return;
    }

    active_stream_task_key_.clear();
}

uint64_t LabJackT7Controller::unixNanosecondsNow() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

void LabJackT7Controller::applyDigitalWrite(const nlohmann::json& arguments) {
    if (!isConnected()) {
        return;
    }

    const auto mappings = parseDigitalWriteMappings(arguments);
    if (demo_mode_) {
        for (const auto& mapping : mappings) {
            const double desired = query(mapping.source, 0.0) != 0.0 ? 1.0 : 0.0;
            upsert(mapping.state, desired);
        }
        return;
    }

    std::lock_guard<std::mutex> lock(handle_mutex_);

    for (const auto& mapping : mappings) {
        const double desired = query(mapping.source, 0.0) != 0.0 ? 1.0 : 0.0;
        int error = LJM_eWriteName(handle_, mapping.labjack_name.c_str(), desired);
        if (error != LJME_NOERROR) {
            handleError(error, "digital_write:" + mapping.labjack_name);
            continue;
        }

        double readback = 0.0;
        error = LJM_eReadName(handle_, mapping.labjack_name.c_str(), &readback);
        if (error != LJME_NOERROR) {
            handleError(error, "digital_readback:" + mapping.labjack_name);
            continue;
        }

        upsert(mapping.state, readback);
    }
}

void LabJackT7Controller::runStreamWorker(DARTWIC::API::TaskRuntime& task_runtime) {
    const auto current_task_key = taskKey(task_runtime);
    if (!tryAcquireStream(current_task_key)) {
        std::string active_task;
        {
            std::lock_guard<std::mutex> stream_lock(stream_mutex_);
            active_task = active_stream_task_key_;
        }
        consoleError(
            "LabJack T7 Stream Already Running [" + instance_name_ + "]",
            "Only one LabJack stream task can own the hardware stream at a time.\n[Active task: " + active_task + "]",
            {},
            "Stop the active stream task before starting another stream.",
            0
        );
        return;
    }

    const auto& arguments = task_runtime.getArguments();
    const auto mappings = parseStreamMappings(arguments);
    if (mappings.empty()) {
        releaseStream(current_task_key);
        return;
    }

    int scans_per_read = arguments.value("scans_per_read", 10);
    if (scans_per_read <= 0) {
        scans_per_read = 10;
    }
    double scan_rate = arguments.value("target_scan_rate", 100.0);
    if (scan_rate <= 0.0) {
        scan_rate = 100.0;
    }
    const double stale_timeout_seconds = std::max(
        1.0,
        2.0 / (scan_rate / static_cast<double>(scans_per_read))
    );
    configureStreamChannelFields(
        mappings,
        stale_timeout_seconds,
        "task:" + task_runtime.getPortalName() + "/" + task_runtime.getTaskName()
    );

    std::vector<int> addresses;
    addresses.reserve(mappings.size());
    for (const auto& mapping : mappings) {
        addresses.push_back(mapping.address);
    }

    std::vector<double> data(static_cast<size_t>(scans_per_read * addresses.size()), 0.0);
    int device_backlog = 0;
    int ljm_backlog = 0;
    int read_number = 0;
    int reads_since_rate_publish = 0;

    publishTaskDiagnostic(task_runtime, "_stream_target_scan_rate", scan_rate);
    publishTaskDiagnostic(task_runtime, "_stream_scans_per_read", static_cast<double>(scans_per_read));
    publishTaskDiagnostic(task_runtime, "_stream_worker_read_rate", 0.0);
    publishTaskDiagnostic(task_runtime, "_stream_last_read_ms", 0.0);

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> demo_distribution(0.0, 10.0);
    uint64_t stream_start = unixNanosecondsNow();
    auto rate_window_start = std::chrono::steady_clock::now();
    auto last_successful_read = std::chrono::steady_clock::now();
    bool stream_started = false;

    while (!task_runtime.isStopRequested()) {
        if (!stream_started) {
            if (demo_mode_) {
                stream_started = true;
            } else if (!isConnected()) {
                const int error = connect();
                publishTaskDiagnostic(
                    task_runtime,
                    "_stream_last_read_ms",
                    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - last_successful_read).count()
                );
                if (error != LJME_NOERROR) {
                    handleError(error, "connect");
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                continue;
            } else {
                double requested_scan_rate = scan_rate;
                int error = LJME_NOERROR;
                {
                    std::lock_guard<std::mutex> handle_lock(handle_mutex_);
                    if (handle_ == -1) {
                        connected_ = false;
                        upsert("device_connected", 0.0);
                        error = LJME_DEVICE_NOT_OPEN;
                    } else {
                        error = LJM_eStreamStart(
                            handle_,
                            scans_per_read,
                            static_cast<int>(addresses.size()),
                            addresses.data(),
                            &requested_scan_rate
                        );
                        if (isStreamAlreadyStartedError(error)) {
                            const int stop_error = LJM_eStreamStop(handle_);
                            if (isHarmlessStreamStopError(stop_error)) {
                                requested_scan_rate = scan_rate;
                                error = LJM_eStreamStart(
                                    handle_,
                                    scans_per_read,
                                    static_cast<int>(addresses.size()),
                                    addresses.data(),
                                    &requested_scan_rate
                                );
                            } else {
                                error = stop_error;
                            }
                        }
                    }
                }
                const auto attempt_end = std::chrono::steady_clock::now();
                publishTaskDiagnostic(
                    task_runtime,
                    "_stream_last_read_ms",
                    std::chrono::duration<double, std::milli>(attempt_end - last_successful_read).count()
                );

                if (error != LJME_NOERROR) {
                    handleError(error, "stream_start");
                    markDisconnectedFromStreamError();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    continue;
                }

                scan_rate = requested_scan_rate;
                stream_started = true;
            }

            read_number = 0;
            reads_since_rate_publish = 0;
            stream_start = unixNanosecondsNow();
            rate_window_start = std::chrono::steady_clock::now();
            last_successful_read = rate_window_start;
            publishTaskDiagnostic(task_runtime, "_stream_actual_scan_rate", scan_rate);
            publishTaskDiagnostic(task_runtime, "_stream_expected_read_rate", scan_rate / static_cast<double>(scans_per_read));
        }

        int error = LJME_NOERROR;
        if (demo_mode_) {
            for (auto& value : data) {
                value = demo_distribution(rng);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>((static_cast<double>(scans_per_read) / scan_rate) * 1000.0)));
        } else {
            std::lock_guard<std::mutex> handle_lock(handle_mutex_);
            error = LJM_eStreamRead(handle_, data.data(), &device_backlog, &ljm_backlog);
        }
        const auto read_end = std::chrono::steady_clock::now();

        if (error != LJME_NOERROR) {
            handleError(error, "stream_read");
            stream_started = false;
            markDisconnectedFromStreamError();
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        publishTaskDiagnostic(
            task_runtime,
            "_stream_last_read_ms",
            std::chrono::duration<double, std::milli>(read_end - last_successful_read).count()
        );
        last_successful_read = read_end;
        ++read_number;
        ++reads_since_rate_publish;
        std::unordered_map<std::string, std::vector<std::pair<double, uint64_t>>> grouped;
        std::unordered_map<std::string, RapidChannel> grouped_channels;
        for (int scan_index = 0; scan_index < scans_per_read; ++scan_index) {
            const double scan_number = static_cast<double>((read_number - 1) * scans_per_read + scan_index);
            const auto timestamp = stream_start + static_cast<uint64_t>((scan_number / scan_rate) * 1'000'000'000.0);
            for (size_t mapping_index = 0; mapping_index < mappings.size(); ++mapping_index) {
                const auto data_index = static_cast<size_t>(scan_index) * mappings.size() + mapping_index;
                const auto& destination = mappings[mapping_index].destination;
                const std::string key = destination.portal + '\x1f' + destination.channel;
                grouped[key].push_back({data[data_index], timestamp});
                grouped_channels[key] = destination;
            }
        }

        for (const auto& [key, values] : grouped) {
            upsertBulk(grouped_channels.at(key), values);
        }

        publishTaskDiagnostic(task_runtime, "_stream_device_scan_backlog", static_cast<double>(device_backlog));
        publishTaskDiagnostic(task_runtime, "_stream_ljm_scan_backlog", static_cast<double>(ljm_backlog));

        const auto rate_window_end = std::chrono::steady_clock::now();
        const double rate_window_seconds = std::chrono::duration<double>(rate_window_end - rate_window_start).count();
        if (rate_window_seconds >= 1.0) {
            publishTaskDiagnostic(
                task_runtime,
                "_stream_worker_read_rate",
                static_cast<double>(reads_since_rate_publish) / rate_window_seconds
            );
            reads_since_rate_publish = 0;
            rate_window_start = rate_window_end;
        }
    }

    if (stream_started && !demo_mode_) {
        std::lock_guard<std::mutex> handle_lock(handle_mutex_);
        if (handle_ != -1) {
            const int error = LJM_eStreamStop(handle_);
            if (!isHarmlessStreamStopError(error)) {
                handleError(error, "stream_stop");
            }
        }
    }

    publishTaskDiagnostic(task_runtime, "_stream_last_read_ms", 0.0);
    releaseStream(current_task_key);
}

void LabJackT7Controller::stopStream() {
    std::string task_key;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        task_key = active_stream_task_key_;
    }

    if (task_key.empty()) {
        return;
    }

    stopStreamTaskKey(task_key);
}

void LabJackT7Controller::stopStream(DARTWIC::API::TaskRuntime& task_runtime) {
    const auto task_key = taskKey(task_runtime);
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        if (active_stream_task_key_ != task_key) {
            return;
        }
    }

    stopStreamTaskKey(task_key);
}

void LabJackT7Controller::stopStreamTaskKey(const std::string& task_key) {
    if (task_key.empty()) {
        return;
    }

    if (!demo_mode_) {
        std::lock_guard<std::mutex> handle_lock(handle_mutex_);
        if (handle_ != -1) {
            const int error = LJM_eStreamStop(handle_);
            if (!isHarmlessStreamStopError(error)) {
                handleError(error, "stream_stop");
            }
        }
    }

    releaseStream(task_key);
}
