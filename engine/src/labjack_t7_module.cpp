#include "labjack_t7_module.h"

#include "labjack_t7_controller.h"

LabJackT7Module::LabJackT7Module(nlohmann::json cfg, DARTWIC::API::SDK_API* drtw)
    : BaseModule(std::move(cfg), drtw),
      instance_name_(getConfig<std::string>("name", "labjackT7")) {
    controller_ = std::make_unique<LabJackT7Controller>(
        this,
        instance_name_,
        getParameter<std::string>("device_type", "T7"),
        getParameter<std::string>("connection_type", "ANY"),
        getParameter<std::string>("identifier", "ANY")
    );
}

LabJackT7Module::~LabJackT7Module() = default;

LabJackT7Controller& LabJackT7Module::controller() {
    return *controller_;
}
