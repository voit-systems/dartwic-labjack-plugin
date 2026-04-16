#include "labjack_t7_plugin.h"

#include "labjack_t7_controller.h"
#include <LabJackM.h>

#include <cmath>
#include <string>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
#endif

#ifdef _WIN32
    #define EXPORT_API __declspec(dllexport)
#else
    #define EXPORT_API __attribute__((visibility("default")))
#endif

namespace {
    std::string getLoadedLjmLibraryPath() {
#ifdef _WIN32
        std::vector<HMODULE> modules(256);
        DWORD bytes_needed = 0;
        if (K32EnumProcessModules(
            GetCurrentProcess(),
            modules.data(),
            static_cast<DWORD>(modules.size() * sizeof(HMODULE)),
            &bytes_needed
        )) {
            const auto module_count = bytes_needed / sizeof(HMODULE);
            modules.resize(module_count);
            for (const auto module_handle : modules) {
                if (GetProcAddress(module_handle, "LJM_ReadLibraryConfigS") == nullptr) {
                    continue;
                }

                std::vector<char> module_path(MAX_PATH);
                while (true) {
                    const DWORD length = K32GetModuleFileNameExA(
                        GetCurrentProcess(),
                        module_handle,
                        module_path.data(),
                        static_cast<DWORD>(module_path.size())
                    );
                    if (length == 0) {
                        break;
                    }
                    if (length < module_path.size() - 1) {
                        return std::string(module_path.data(), length);
                    }
                    module_path.resize(module_path.size() * 2);
                }
            }
        }

        HMODULE module_handle = nullptr;
        const BOOL found_module = GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&LJM_ReadLibraryConfigS),
            &module_handle
        );
        if (!found_module) {
            module_handle = GetModuleHandleA("LabJackM.dll");
        }
        if (!module_handle) {
            return "";
        }

        std::vector<char> path_buffer(MAX_PATH);
        while (true) {
            const DWORD length = GetModuleFileNameA(module_handle, path_buffer.data(), static_cast<DWORD>(path_buffer.size()));
            if (length == 0) {
                return "";
            }
            if (length < path_buffer.size() - 1) {
                return std::string(path_buffer.data(), length);
            }
            path_buffer.resize(path_buffer.size() * 2);
        }
#else
        return "";
#endif
    }

    std::string ljmErrorText(int error_number) {
        char error_string[LJM_MAX_NAME_SIZE] = "LJM error not found.";
        LJM_ErrorToString(error_number, error_string);
        return error_string;
    }

    nlohmann::json buildLjmInfoPayload(const nlohmann::json& request_payload) {
        nlohmann::json payload = {
            {"plugin_sdk_version", LJM_VERSION},
            {"required_ljm_version", LJM_VERSION},
            {"system_runtime_version", nullptr},
            {"version_match", false},
            {"constants_ok", false},
            {"library_ready", false},
            {"loaded_library_path", getLoadedLjmLibraryPath()}
        };

        if (request_payload.contains("module_instance_name") && request_payload["module_instance_name"].is_string()) {
            payload["module_instance_name"] = request_payload["module_instance_name"];
        }

        double runtime_version = 0.0;
        int error = LJM_ReadLibraryConfigS(LJM_LIBRARY_VERSION, &runtime_version);
        if (error != LJME_NOERROR) {
            payload["ljm_error_number"] = error;
            payload["ljm_error"] = ljmErrorText(error);
            payload["error"] = "The LabJack LJM system install could not be read.";
            return payload;
        }

        payload["system_runtime_version"] = runtime_version;
        const bool version_match = std::abs(runtime_version - LJM_VERSION) <= 0.00001;
        payload["version_match"] = version_match;

        int address = 0;
        int type = 0;
        error = LJM_NameToAddress("AIN0", &address, &type);
        if (error != LJME_NOERROR) {
            payload["ljm_error_number"] = error;
            payload["ljm_error"] = ljmErrorText(error);
            payload["error"] = "The LabJack LJM constants/configuration files could not resolve AIN0.";
            return payload;
        }

        payload["constants_ok"] = true;
        payload["library_ready"] = version_match;
        if (!version_match) {
            payload["error"] = "The installed LabJack LJM runtime version does not match the plugin SDK version.";
        }
        return payload;
    }
}

void LabJackT7Plugin::onPluginLoaded() {
    dartwic->registerOperation("labjack_t7/get-ljm-info", [](const nlohmann::json& payload) {
        return buildLjmInfoPayload(payload);
    });

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
        const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
        auto module = dartwic->getModuleInstance(instance_name);
        auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
        if (!labjack_module) {
            return;
        }
        labjack_module->controller().applyDigitalWrite(task_runtime.getArguments());
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
                {"negative_channel", 199},
                {"range", 10.0},
                {"channel", ""}
            }
        })}
    };
    stream_task.on_task = [this](const DARTWIC::API::TaskTypeDefinition&, DARTWIC::API::TaskRuntime& task_runtime, double) {
        const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
        auto module = dartwic->getModuleInstance(instance_name);
        auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
        if (!labjack_module) {
            return;
        }
        labjack_module->controller().runStreamWorker(task_runtime);
    };
    stream_task.on_end = [this](const DARTWIC::API::TaskTypeDefinition&, DARTWIC::API::TaskRuntime& task_runtime) {
        const std::string instance_name = task_runtime.getArguments().value("module_instance_name", std::string{});
        auto module = dartwic->getModuleInstance(instance_name);
        auto labjack_module = std::dynamic_pointer_cast<LabJackT7Module>(module);
        if (labjack_module) {
            labjack_module->controller().stopStream(task_runtime);
        }
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
