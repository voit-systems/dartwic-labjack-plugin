#ifndef BASEPLUGIN_H
#define BASEPLUGIN_H

#include <modules/BaseModule.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace DARTWIC::Plugins {
    struct PluginModuleType {
        std::string id;
        std::string config_path = "module_config.json";
        std::string default_parameters_path = "default_parameters.json";
    };

    class BasePlugin {
    public:
        virtual ~BasePlugin() = default;
        BasePlugin(nlohmann::json cfg, API::SDK_API* drtw) : config(std::move(cfg)), dartwic(drtw) {}

        virtual void onPluginLoaded() {}

        virtual std::vector<PluginModuleType> getModuleTypes() const {
            return {};
        }

        virtual Modules::BaseModule* createModule(
            const std::string& module_type_id,
            nlohmann::json cfg,
            API::SDK_API* drtw
        ) {
            (void)module_type_id;
            (void)cfg;
            (void)drtw;
            return nullptr;
        }

        void setPluginId(const std::string& plugin_id_value) {
            plugin_id = plugin_id_value;
        }

        const std::string& getPluginId() const {
            return plugin_id;
        }

        nlohmann::json config = nlohmann::json::object();
        API::SDK_API* dartwic = nullptr;

    protected:
        std::string plugin_id;
    };
}

#endif //BASEPLUGIN_H
