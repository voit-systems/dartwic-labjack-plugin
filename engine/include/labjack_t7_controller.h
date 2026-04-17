#ifndef LABJACK_T7_CONTROLLER_H
#define LABJACK_T7_CONTROLLER_H

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <LabJackM.h>
#include <sdk/sdk_api.h>

class LabJackT7Module;

class LabJackT7Controller {
public:
    LabJackT7Controller(
        LabJackT7Module* module,
        std::string instance_name,
        std::string device_type,
        std::string connection_type,
        std::string identifier
    );
    ~LabJackT7Controller();

    bool isConnected() const;
    void applyDigitalWrite(const nlohmann::json& arguments);
    void runStreamWorker(DARTWIC::API::TaskRuntime& task_runtime);
    void stopStream();
    void stopStream(DARTWIC::API::TaskRuntime& task_runtime);

private:
    struct RapidChannel {
        std::string portal;
        std::string channel;
    };

    struct StreamMapping {
        std::string labjack_name;
        RapidChannel destination;
        int address = 0;
        bool is_analog = true;
        int register_number = 0;
        int negative_channel = 199;
        double range = 10.0;
    };

    struct DigitalWriteMapping {
        std::string labjack_name;
        RapidChannel source;
        RapidChannel state;
        int register_number = 0;
    };

    double query(const std::string& channel, double default_value = 0.0) const;
    double query(const RapidChannel& channel, double default_value = 0.0) const;
    void upsert(const std::string& channel, DARTWIC::API::ChannelValue value) const;
    void upsert(const RapidChannel& channel, DARTWIC::API::ChannelValue value) const;
    void upsertBulk(const RapidChannel& channel, const std::vector<std::pair<double, uint64_t>>& data) const;
    void consoleError(const std::string& title, const std::string& description, std::vector<std::string> channels, const std::string& resolution, int auto_ack = 0) const;

    void connectionLoopStart();
    void connectionLoop();
    void connectionLoopEnd();
    bool validateLjmLibrary() const;
    int connect();
    void disconnect();
    void verifyConnection();
    void handleError(int error_number, const std::string& operation) const;

    std::optional<RapidChannel> parseRapidChannel(const nlohmann::json& value) const;
    std::optional<RapidChannel> splitRapidChannelPath(const std::string& channel_path) const;
    std::string buildStreamLabJackName(const nlohmann::json& mapping) const;
    std::string buildDigitalLabJackName(const nlohmann::json& mapping) const;
    std::vector<StreamMapping> parseStreamMappings(const nlohmann::json& arguments, std::vector<std::string>& errors);
    std::vector<DigitalWriteMapping> parseDigitalWriteMappings(const nlohmann::json& arguments) const;
    bool validateAnalogStreamMapping(const StreamMapping& mapping, std::vector<std::string>& errors) const;
    bool isValidAnalogRange(double range) const;
    void handleStreamConfigError(const std::vector<std::string>& errors, DARTWIC::API::TaskRuntime& task_runtime) const;
    int applyAnalogStreamConfigLocked(const std::vector<StreamMapping>& mappings, std::string& operation);
    void publishTaskDiagnostic(DARTWIC::API::TaskRuntime& task_runtime, const std::string& suffix, DARTWIC::API::ChannelValue value) const;
    void configureObserveOnlyChannel(const std::string& portal, const std::string& channel, const std::string& controller) const;
    void configureStreamChannelFields(const std::vector<StreamMapping>& mappings, double stale_timeout_seconds, const std::string& controller) const;
    void markDisconnectedFromStreamError();
    bool tryAcquireStream(const std::string& task_key);
    void releaseStream(const std::string& task_key);
    void stopStreamTaskKey(const std::string& task_key);
    uint64_t unixNanosecondsNow() const;

    LabJackT7Module* module_ = nullptr;
    std::string instance_name_;
    std::string device_type_;
    std::string connection_type_;
    std::string identifier_;
    std::string connection_loop_name_;
    bool ljm_library_ready_ = true;
    std::atomic_bool demo_mode_{false};
    std::atomic_bool connected_{false};
    std::mutex handle_mutex_;
    std::mutex stream_mutex_;
    std::string active_stream_task_key_;
    int handle_ = -1;
};

#endif
