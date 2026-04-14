#ifndef BASEMODULE_H
#define BASEMODULE_H

#include <nlohmann/json.hpp>

#include "sdk_api.h"

namespace DARTWIC::Modules {

    class BaseModule {
    public:
        virtual ~BaseModule() = default;
        BaseModule(nlohmann::json cfg, API::SDK_API* drtw) : config(std::move(cfg)), dartwic(drtw) {}

        template <typename T>
        T getConfig(const std::string& key, const T& default_value = T()) const {
            if (config.is_object() && config.contains(key) && !config.at(key).is_null()) {
                try {
                    return config.at(key).get<T>();
                } catch (...) {
                }
            }
            return default_value;
        }

        template <typename T>
        T getParameter(const std::string& key, const T& default_value = T()) const {
            if (
                config.is_object() &&
                config.contains("parameters") &&
                config.at("parameters").is_object() &&
                config.at("parameters").contains(key) &&
                !config.at("parameters").at(key).is_null()
            ) {
                try {
                    return config.at("parameters").at(key).get<T>();
                } catch (...) {
                }
            }
            return default_value;
        }

        void setPluginId(const std::string& plugin_id_value) {
            plugin_id = plugin_id_value;
        }

        const std::string& getPluginId() const {
            return plugin_id;
        }

        void setModuleTypeId(const std::string& module_type_id_value) {
            module_type_id = module_type_id_value;
        }

        const std::string& getModuleTypeId() const {
            return module_type_id;
        }

        nlohmann::json config = nlohmann::json::object();
        API::SDK_API* dartwic = nullptr;

    protected:
        std::string plugin_id;
        std::string module_type_id;
    };
}

#endif //BASEMODULE_H
