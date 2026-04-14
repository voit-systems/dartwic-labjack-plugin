#include "labjack_t7_controller.h"

#include "labjack_t7_module.h"

#include <chrono>
#include <cstring>
#include <random>
#include <sstream>
#include <thread>

using DARTWIC::API::ChannelField;
using DARTWIC::API::ChannelValue;

LabJackT7Controller::LabJackT7Controller(
    LabJackT7Module* module,
    std::string instance_name,
    std::string device_type,
    std::string connection_type,
    std::string identifier,
    std::string default_stream_channels
) : module_(module),
    instance_name_(std::move(instance_name)),
    device_type_(std::move(device_type)),
    connection_type_(std::move(connection_type)),
    identifier_(std::move(identifier)),
    default_stream_channels_(std::move(default_stream_channels)),
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

void LabJackT7Controller::insert(const std::string& channel, ChannelValue value) const {
    module_->dartwic->insertChannelField(instance_name_, channel, ChannelField::VALUE, std::move(value));
}

void LabJackT7Controller::upsert(const std::string& channel, ChannelValue value) const {
    module_->dartwic->upsertChannelField(instance_name_, channel, ChannelField::VALUE, std::move(value));
}

void LabJackT7Controller::upsertBulk(const std::string& channel, const std::vector<std::pair<double, uint64_t>>& data) const {
    module_->dartwic->upsertChannelValueBulk(instance_name_, channel, data);
}

void LabJackT7Controller::consoleError(const std::string& title, const std::string& description, std::vector<std::string> channels, const std::string& resolution, int auto_ack) const {
    module_->dartwic->consoleError(title, description, std::move(channels), resolution, auto_ack);
}

void LabJackT7Controller::connectionLoopStart() {
    upsert("device.connected", 0.0);
    upsert("stream.running", 0.0);
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
    demo_mode_ = std::strcmp(identifier_.c_str(), LJM_DEMO_MODE) == 0;

    const int error = LJM_OpenS(device_type_.c_str(), connection_type_.c_str(), identifier_.c_str(), &handle_);
    if (error != LJME_NOERROR) {
        handle_ = -1;
        connected_ = false;
        upsert("device.connected", 0.0);
        return error;
    }

    connected_ = true;
    upsert("device.connected", 1.0);
    return LJME_NOERROR;
}

void LabJackT7Controller::disconnect() {
    std::lock_guard<std::mutex> lock(handle_mutex_);
    if (handle_ != -1) {
        LJM_Close(handle_);
        handle_ = -1;
    }
    connected_ = false;
    upsert("device.connected", 0.0);
}

void LabJackT7Controller::verifyConnection() {
    if (demo_mode_) {
        connected_ = true;
        upsert("device.connected", 1.0);
        return;
    }

    double value = 0.0;
    const int error = LJM_eReadName(handle_, "SERIAL_NUMBER", &value);
    if (error == LJME_NOERROR) {
        connected_ = true;
        upsert("device.connected", 1.0);
        return;
    }

    connected_ = false;
    upsert("device.connected", 0.0);
    handleError(error, "verify_connection");
}

void LabJackT7Controller::handleError(int error_number, const std::string& operation) {
    char error_string[LJM_MAX_NAME_SIZE] = "LJM error not found.";
    LJM_ErrorToString(error_number, error_string);
    consoleError(
        "LabJack T7 Error [" + instance_name_ + "]",
        "LabJack operation failed.\n[Operation: " + operation + "]\n[LJM Error: " + std::string(error_string) + "]",
        {instance_name_ + "/device.connected.value"},
        "Check the LabJack connection, stream configuration, and device state.",
        5
    );
}

void LabJackT7Controller::applyDigitalWrite(const nlohmann::json& arguments) {
    if (!isConnected() || demo_mode_) {
        return;
    }

    std::vector<std::string> channels = {"DIO8", "DIO9", "DIO10", "DIO11", "DIO12", "DIO13", "DIO14", "DIO15", "DIO17", "DIO18", "DIO19"};
    if (arguments.contains("channels") && arguments["channels"].is_array()) {
        channels.clear();
        for (const auto& item : arguments["channels"]) {
            if (item.is_string()) {
                channels.push_back(item.get<std::string>());
            } else if (item.is_object() && item.contains("channel") && item["channel"].is_string()) {
                channels.push_back(item["channel"].get<std::string>());
            }
        }
    }

    std::lock_guard<std::mutex> lock(handle_mutex_);
    for (const auto& channel : channels) {
        const double desired = query("digital_channels." + channel + ".desired_state", 0.0);
        const double expected = query("digital_channels." + channel + ".expected_state", 0.0);
        if (desired == expected) {
            continue;
        }

        const int error = LJM_eWriteName(handle_, channel.c_str(), desired);
        if (error == LJME_NOERROR) {
            upsert("digital_channels." + channel + ".expected_state", desired);
        } else {
            handleError(error, "digital_write:" + channel);
        }
    }
}

std::vector<std::string> LabJackT7Controller::parseChannelList(const nlohmann::json& arguments) const {
    if (arguments.contains("channels") && arguments["channels"].is_array()) {
        std::vector<std::string> channels;
        for (const auto& item : arguments["channels"]) {
            if (item.is_string()) {
                channels.push_back(item.get<std::string>());
            }
        }
        return channels;
    }

    if (arguments.contains("channels") && arguments["channels"].is_string()) {
        return splitCommaSeparated(arguments["channels"].get<std::string>());
    }

    return splitCommaSeparated(default_stream_channels_);
}

std::vector<std::string> LabJackT7Controller::splitCommaSeparated(const std::string& input) const {
    std::vector<std::string> result;
    std::stringstream stream(input);
    std::string item;
    while (std::getline(stream, item, ',')) {
        item.erase(0, item.find_first_not_of(" \t"));
        const auto end = item.find_last_not_of(" \t");
        if (end != std::string::npos) {
            item.erase(end + 1);
        }
        if (!item.empty()) {
            result.push_back(item);
        }
    }
    return result;
}

std::vector<int> LabJackT7Controller::resolveAddresses(const std::vector<std::string>& channels) {
    std::vector<int> addresses;
    addresses.reserve(channels.size());
    for (const auto& channel : channels) {
        int address = 0;
        int type = 0;
        const int error = LJM_NameToAddress(channel.c_str(), &address, &type);
        if (error == LJME_NOERROR) {
            addresses.push_back(address);
        } else {
            handleError(error, "resolve_stream_channel:" + channel);
        }
    }
    return addresses;
}

uint64_t LabJackT7Controller::unixNanosecondsNow() const {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

void LabJackT7Controller::runStreamWorker(DARTWIC::API::TaskRuntime& task_runtime) {
    std::unique_lock<std::mutex> stream_lock(stream_mutex_, std::try_to_lock);
    if (!stream_lock.owns_lock()) {
        consoleError(
            "LabJack T7 Stream Already Running [" + instance_name_ + "]",
            "Only one LabJack stream task can own the hardware stream at a time.",
            {instance_name_ + "/stream.running.value"},
            "Stop the active stream task before starting another stream.",
            5
        );
        return;
    }

    const auto& arguments = task_runtime.getArguments();
    const std::vector<std::string> channels = parseChannelList(arguments);
    const std::vector<int> addresses = resolveAddresses(channels);
    if (channels.empty() || addresses.empty() || addresses.size() != channels.size()) {
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

    std::vector<double> data(static_cast<size_t>(scans_per_read * addresses.size()), 0.0);
    int device_backlog = 0;
    int ljm_backlog = 0;
    int read_number = 0;

    {
        std::lock_guard<std::mutex> handle_lock(handle_mutex_);
        if (!isConnected()) {
            return;
        }

        if (!demo_mode_) {
            const int error = LJM_eStreamStart(handle_, scans_per_read, static_cast<int>(addresses.size()), const_cast<int*>(addresses.data()), &scan_rate);
            if (error != LJME_NOERROR) {
                handleError(error, "stream_start");
                return;
            }
        }
    }

    stream_running_ = true;
    upsert("stream.running", 1.0);
    upsert("stream.actual_scan_rate", scan_rate);

    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<double> demo_distribution(0.0, 10.0);
    const auto stream_start = unixNanosecondsNow();

    while (!task_runtime.isStopRequested()) {
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

        if (error != LJME_NOERROR) {
            handleError(error, "stream_read");
            break;
        }

        ++read_number;
        std::unordered_map<std::string, std::vector<std::pair<double, uint64_t>>> grouped;
        for (int scan_index = 0; scan_index < scans_per_read; ++scan_index) {
            const double scan_number = static_cast<double>((read_number - 1) * scans_per_read + scan_index);
            const auto timestamp = stream_start + static_cast<uint64_t>((scan_number / scan_rate) * 1'000'000'000.0);
            for (size_t channel_index = 0; channel_index < channels.size(); ++channel_index) {
                const auto data_index = static_cast<size_t>(scan_index) * channels.size() + channel_index;
                grouped["stream_channels." + channels[channel_index]].push_back({data[data_index], timestamp});
            }
        }

        for (const auto& [channel, values] : grouped) {
            upsertBulk(channel, values);
        }

        upsert("stream.device_scan_backlog", static_cast<double>(device_backlog));
        upsert("stream.ljm_scan_backlog", static_cast<double>(ljm_backlog));
        upsert("stream.read_number", static_cast<double>(read_number));
    }

    stopStream();
}

void LabJackT7Controller::stopStream() {
    if (!stream_running_.exchange(false)) {
        upsert("stream.running", 0.0);
        return;
    }

    if (!demo_mode_) {
        std::lock_guard<std::mutex> handle_lock(handle_mutex_);
        if (handle_ != -1) {
            const int error = LJM_eStreamStop(handle_);
            if (error != LJME_NOERROR && error != LJME_STREAM_NOT_INITIALIZED) {
                handleError(error, "stream_stop");
            }
        }
    }

    upsert("stream.running", 0.0);
}
