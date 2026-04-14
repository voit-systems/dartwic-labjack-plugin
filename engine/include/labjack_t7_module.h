#ifndef LABJACK_T7_MODULE_H
#define LABJACK_T7_MODULE_H

#include <memory>
#include <modules/BaseModule.h>

class LabJackT7Controller;

class LabJackT7Module : public DARTWIC::Modules::BaseModule {
public:
    LabJackT7Module(nlohmann::json cfg, DARTWIC::API::SDK_API* drtw);
    ~LabJackT7Module() override;

    LabJackT7Controller& controller();

private:
    std::string instance_name_;
    std::unique_ptr<LabJackT7Controller> controller_;
};

#endif
