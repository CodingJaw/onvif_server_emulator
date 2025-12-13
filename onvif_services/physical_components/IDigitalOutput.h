#pragma once

#include "IPhysicalComponent.h"

#include <memory>
#include <string>
#include <vector>

class IDigitalOutput : public IPhysicalComponent
{
public:
    void Enable() override;
    void Disable() override;

    bool IsEnabled() override
    {
        return is_enabled_;
    }

    bool GetState() override
    {
        return state_;
    }

    bool SetState(bool state) override
    {
        return state_ = state;
    }

    bool InvertState() override;

    void SetToken(std::string str) { token_ = str; }

    std::string GetToken()
    {
        return token_;
    }

protected:
    std::string token_;

    bool state_ = false;

    bool is_enabled_ = true;
};

class SimpleDigitalOutputImpl : public IDigitalOutput
{
public:
    SimpleDigitalOutputImpl(const std::string& token, bool state = false)
    {
        token_ = token;
        state_ = state;
    }
};

using DigitalOutputsList = std::vector<std::shared_ptr<IDigitalOutput>>;

