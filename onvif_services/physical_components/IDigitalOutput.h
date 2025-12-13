#pragma once

#include "IPhysicalComponent.h"

#include <memory>
#include <chrono>
#include <string>
#include <vector>

enum class RelayMode
{
        Bistable,
        Monostable,
};

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

        void SetIdleState(bool idle_state)
        {
                idle_state_ = idle_state;
        }

        bool GetIdleState() const
        {
                return idle_state_;
        }

        void SetMode(RelayMode mode)
        {
                mode_ = mode;
        }

        RelayMode GetMode() const
        {
                return mode_;
        }

        void SetDelayTime(std::chrono::milliseconds delay)
        {
                delay_time_ = delay;
        }

        std::chrono::milliseconds GetDelayTime() const
        {
                return delay_time_;
        }

        void SetPulseTime(std::chrono::milliseconds pulse)
        {
                pulse_time_ = pulse;
        }

        std::chrono::milliseconds GetPulseTime() const
        {
                return pulse_time_;
        }

        void SetToken(std::string str) { token_ = std::move(str); };

        std::string GetToken()
        {
                return token_;
        }

protected:
        std::string token_;

        bool idle_state_ = false;
        RelayMode mode_ = RelayMode::Bistable;
        std::chrono::milliseconds delay_time_{0};
        std::chrono::milliseconds pulse_time_{0};

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

