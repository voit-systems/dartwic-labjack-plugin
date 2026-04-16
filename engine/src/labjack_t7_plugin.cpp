#include "labjack_t7_plugin.h"

#include "labjack_t7_controller.h"

#include <exception>
#include <iostream>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <eh.h>
#endif

#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

namespace {
#ifdef _WIN32
    class StructuredException final : public std::exception {
    public:
        StructuredException(unsigned int code, void* address)
            : code_(code), address_(address) {}

        const char* what() const noexcept override {
            return "structured exception";
        }

        unsigned int code() const noexcept {
            return code_;
        }

        void* address() const noexcept {
            return address_;
        }

    private:
        unsigned int code_ = 0;
        void* address_ = nullptr;
    };

    void translateStructuredException(unsigned int code, _EXCEPTION_POINTERS* info) {
        void* address = nullptr;
        if (info && info->ExceptionRecord) {
            address = info->ExceptionRecord->ExceptionAddress;
        }
        throw StructuredException(code, address);
    }
#endif

    template <typename Callback>
    void runGuarded(const char* operation, Callback&& callback) {
#ifdef _WIN32
        const auto previous_translator = _set_se_translator(translateStructuredException);
#endif
        try {
            callback();
        } catch (const StructuredException& error) {
            std::cerr
                << "LabJack T7 guarded task exited operation=" << operation
                << " error=structured exception code=0x" << std::hex << error.code() << std::dec
                << " address=" << error.address()
                << std::endl;
        } catch (const std::exception& error) {
            std::cerr << "LabJack T7 guarded task exited operation=" << operation << " error=" << error.what() << std::endl;
        } catch (...) {
            std::cerr << "LabJack T7 guarded task exited operation=" << operation << " error=unknown" << std::endl;
        }
#ifdef _WIN32
        _set_se_translator(previous_translator);
#endif
    }
}

void LabJackT7Plugin::onPluginLoaded() {
    DARTWIC::API::TaskTypeDefinition digital_write_task;
    digital_write_task.metadata.task_type = "labjack.digital_write";
    digital_write_task.metadata.structure = DARTWIC::API::TaskStructure::Periodic;
    digital_write_task.metadata.icon_url = "https://labjack.com/cdn/shop/files/LabJack_Logo_fa79278b-4a0c-4e5e-8ed7-dff9ad5b8dbb.png?v=1656709585&width=180";
    digital_write_task.metadata.exposed_from = "labjack_t7";
    digital_write_task.metadata.expected_plugin_id = "labjack_t7";
    digital_write_task.metadata.default_arguments = {
        {"module_instance_name", ""},
        {"mappings", nlohmann::json::array({
            {
                {"register", 0},
                {"channel", ""}
            }
        })}
    };
    digital_write_task.on_task = [this](const DARTWIC::API::TaskTypeDefinition&, DARTWIC::API::TaskRuntime& task_runtime, double) {
        runGuarded("digital_write", [&]() {
            const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
            auto module = dartwic->getModuleInstance(instance_name);
            auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
            if (!labjack_module) {
                return;
            }
            labjack_module->controller().applyDigitalWrite(task_runtime.getArguments());
        });
    };
    digital_write_task.cleanup = [](DARTWIC::API::TaskRuntime&) {};
    dartwic->registerTaskType(digital_write_task);

    DARTWIC::API::TaskTypeDefinition stream_task;
    stream_task.metadata.task_type = "labjack.stream";
    stream_task.metadata.structure = DARTWIC::API::TaskStructure::Worker;
    stream_task.metadata.icon_url = "https://labjack.com/cdn/shop/files/LabJack_Logo_fa79278b-4a0c-4e5e-8ed7-dff9ad5b8dbb.png?v=1656709585&width=180";
    stream_task.metadata.exposed_from = "labjack_t7";
    stream_task.metadata.expected_plugin_id = "labjack_t7";
    stream_task.metadata.default_arguments = {
        {"module_instance_name", ""},
        {"target_scan_rate", 100.0},
        {"scans_per_read", 10},
        {"mappings", nlohmann::json::array({
            {
                {"channel_type", "analog"},
                {"register", 0},
                {"channel", ""}
            }
        })}
    };
    stream_task.on_task = [this](const DARTWIC::API::TaskTypeDefinition&, DARTWIC::API::TaskRuntime& task_runtime, double) {
        runGuarded("stream_worker", [&]() {
            const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
            auto module = dartwic->getModuleInstance(instance_name);
            auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
            if (!labjack_module) {
                return;
            }
            labjack_module->controller().runStreamWorker(task_runtime);
        });
    };
    stream_task.on_end = [this](const DARTWIC::API::TaskTypeDefinition&, DARTWIC::API::TaskRuntime& task_runtime) {
        runGuarded("stream_stop", [&]() {
            const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
            auto module = dartwic->getModuleInstance(instance_name);
            auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
            if (labjack_module) {
                labjack_module->controller().stopStream(task_runtime);
            }
        });
    };
    stream_task.cleanup = [](DARTWIC::API::TaskRuntime&) {};
    dartwic->registerTaskType(stream_task);
}

DARTWIC::Modules::BaseModule* LabJackT7Plugin::createModule(
    const std::string& module_type_id,
    nlohmann::json cfg,
    DARTWIC::API::SDK_API* drtw
) {
    if (module_type_id != "labjack_t7") {
        return nullptr;
    }
    return new LabJackT7Module(std::move(cfg), drtw);
}

extern "C" EXPORT_API DARTWIC::Plugins::BasePlugin* createPlugin(nlohmann::json cfg, DARTWIC::API::SDK_API* drtw) {
    return new LabJackT7Plugin(std::move(cfg), drtw);
}
