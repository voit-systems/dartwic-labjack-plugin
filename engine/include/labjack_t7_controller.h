#ifndef LABJACK_T7_CONTROLLER_H
#define LABJACK_T7_CONTROLLER_H

#include <atomic>
#include <mutex>
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
        std::string identifier,
        std::string default_stream_channels
    );
    ~LabJackT7Controller();

    bool isConnected() const;
    void applyDigitalWrite(const nlohmann::json& arguments);
    void runStreamWorker(DARTWIC::API::TaskRuntime& task_runtime);
    void stopStream();

private:
    double query(const std::string& channel, double default_value = 0.0) const;
    void insert(const std::string& channel, DARTWIC::API::ChannelValue value) const;
    void upsert(const std::string& channel, DARTWIC::API::ChannelValue value) const;
    void upsertBulk(const std::string& channel, const std::vector<std::pair<double, uint64_t>>& data) const;
    void consoleError(const std::string& title, const std::string& description, std::vector<std::string> channels, const std::string& resolution, int auto_ack = 5) const;

    void connectionLoopStart();
    void connectionLoop();
    void connectionLoopEnd();
    int connect();
    void disconnect();
    void verifyConnection();
    void handleError(int error_number, const std::string& operation);

    std::vector<std::string> parseChannelList(const nlohmann::json& arguments) const;
    std::vector<std::string> splitCommaSeparated(const std::string& input) const;
    std::vector<int> resolveAddresses(const std::vector<std::string>& channels);
    uint64_t unixNanosecondsNow() const;

    LabJackT7Module* module_ = nullptr;
    std::string instance_name_;
    std::string device_type_;
    std::string connection_type_;
    std::string identifier_;
    std::string default_stream_channels_;
    std::string connection_loop_name_;
    std::atomic_bool demo_mode_{false};
    std::atomic_bool connected_{false};
    std::atomic_bool stream_running_{false};
    std::mutex handle_mutex_;
    std::mutex stream_mutex_;
    int handle_ = -1;
};

#endif
