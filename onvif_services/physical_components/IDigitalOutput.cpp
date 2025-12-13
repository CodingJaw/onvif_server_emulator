#include "IDigitalOutput.h"

void IDigitalOutput::Enable()
{
        is_enabled_ = true;
}

void IDigitalOutput::Disable()
{
        is_enabled_ = false;
}

bool IDigitalOutput::InvertState()
{
        state_ = !state_;
        return state_;
}

