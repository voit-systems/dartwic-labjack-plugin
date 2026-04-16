#ifndef SDK_API_H
#define SDK_API_H

#include <cstdint>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace DARTWIC::Modules {
    class BaseModule;
}

namespace DARTWIC::API {
    enum class TaskStructure {
        Unknown,
        Periodic,
        Worker
    };

    enum class ChannelField {
        VALUE,
        COMMANDED_BY,
        TIMESTAMP,
        UNITS,
        SCALE,
        OFFSET,
        STALE_TIMEOUT,
        MAPPED_CHANNEL,
        RECORD_MODE,
        MEAN,
        STDEV,
        BUFFER_SIZE,
        DATA_FRAME,
        CONTROL_POLICY,
        CONTROL_OWNER,
        ACTIVE_CONTROLLER
    };

    enum class RecordMode {
        OnValueChange,
        Never,
        EveryValue
    };

    enum class ControlPolicy {
        Free,
        Automatic,
        ManualOverride,
        ObserveOnly
    };

    using ChannelValue = std::variant<double, int, std::string, bool, RecordMode, ControlPolicy>;

    struct TaskTypeDefinition;

    struct TaskTypeMetadata {
        std::string task_type;
        TaskStructure structure = TaskStructure::Unknown;
        std::string icon_url;
        std::string exposed_from;
        std::string expected_plugin_id;
        nlohmann::json default_arguments = nlohmann::json::object();
    };

    class TaskRuntime {
    public:
        virtual ~TaskRuntime() = default;

        virtual const std::string& getPortalName() const = 0;
        virtual const std::string& getTaskName() const = 0;
        virtual const std::string& getTaskType() const = 0;
        virtual const nlohmann::json& getMetadata() const = 0;
        virtual const nlohmann::json& getArguments() const = 0;
        virtual double getElapsedSeconds() const = 0;
        virtual bool isStopRequested() const = 0;

        virtual void setRuntimeContext(const std::string& key, std::shared_ptr<void> value) = 0;
        virtual std::shared_ptr<void> getRuntimeContext(const std::string& key) const = 0;
        virtual void removeRuntimeContext(const std::string& key) = 0;
        virtual void clearRuntimeContext() = 0;

        template <typename T>
        void setTypedRuntimeContext(const std::string& key, const std::shared_ptr<T>& value) {
            setRuntimeContext(key, std::static_pointer_cast<void>(value));
        }

        template <typename T>
        std::shared_ptr<T> getTypedRuntimeContext(const std::string& key) const {
            return std::static_pointer_cast<T>(getRuntimeContext(key));
        }
    };

    using TaskLifecycleFunction = std::function<void(const TaskTypeDefinition&, TaskRuntime&)>;
    using TaskLoopFunction = std::function<void(const TaskTypeDefinition&, TaskRuntime&, double)>;
    using TaskCleanupFunction = std::function<void(TaskRuntime&)>;

    struct TaskTypeDefinition {
        TaskTypeMetadata metadata;
        TaskLifecycleFunction on_start;
        TaskLoopFunction on_task;
        TaskLifecycleFunction on_end;
        TaskCleanupFunction cleanup;
    };

    struct ModuleInstanceSummary {
        std::string name;
        std::string plugin_id;
        std::string module_type_id;
    };

    class SDK_API {
    public:
        SDK_API() = default;
        virtual ~SDK_API() = default;

        virtual double queryChannelField(const std::string& portal,
            const std::string& channel,
            ChannelField field,
            std::optional<ChannelValue> default_value) = 0;

        virtual void insertChannelField(const std::string& portal,
            const std::string& channel,
            ChannelField field,
            ChannelValue value) = 0;

        virtual void upsertChannelField(const std::string& portal,
            const std::string& channel,
            ChannelField field,
            ChannelValue value) = 0;

        virtual bool removeChannel(const std::string& portal, const std::string& channel) = 0;

        virtual void onStart(const std::string& loop_name, const std::function<void()>& function) = 0;
        virtual void onLoop(const std::string& loop_name, const std::function<void()>& function) = 0;
        virtual void onEnd(const std::string& loop_name, const std::function<void()>& function) = 0;
        virtual void removeLoop(const std::string& loop_name) = 0;

        virtual int consoleError(
            std::string message_title,
            std::string message_description,
            std::vector<std::string> tags,
            std::string resolution,
            int auto_acknowledge = 0
        ) = 0;

        virtual void registerTaskType(const TaskTypeDefinition& task_type_definition) = 0;
        virtual std::shared_ptr<Modules::BaseModule> getModuleInstance(const std::string& instance_name) = 0;
        virtual std::vector<ModuleInstanceSummary> getModuleInstances(const std::string& plugin_id = "") = 0;

        virtual void upsertChannelValueBulk(const std::string& portal,
            const std::string& channel,
            const std::vector<std::pair<double, uint64_t>>& data) = 0;
    };
}

#endif //SDK_API_H
