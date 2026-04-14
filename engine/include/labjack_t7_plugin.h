#ifndef LABJACK_T7_PLUGIN_H
#define LABJACK_T7_PLUGIN_H

#include <plugins/BasePlugin.h>

#include "labjack_t7_module.h"

class LabJackT7Plugin : public DARTWIC::Plugins::BasePlugin {
public:
    LabJackT7Plugin(nlohmann::json cfg, DARTWIC::API::SDK_API* drtw)
        : BasePlugin(std::move(cfg), drtw) {}

    void onPluginLoaded() override;

    std::vector<DARTWIC::Plugins::PluginModuleType> getModuleTypes() const override {
        return {
            {"labjack_t7"}
        };
    }

    DARTWIC::Modules::BaseModule* createModule(
        const std::string& module_type_id,
        nlohmann::json cfg,
        DARTWIC::API::SDK_API* drtw
    ) override;
};

#endif
