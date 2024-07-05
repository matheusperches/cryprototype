// FlightModifierFlags.h
#pragma once
#include <cstdint>

// Define the Flight Modifier Flags enum
enum class EFlightModifierFlag : uint8_t
{
    Newtonian = 1 << 0,
    Coupled = 1 << 1,
    Boost = 1 << 2,
    Gravity = 1 << 3,
    Comstab = 1 << 4
};

// Utility class for handling the bitwise flags
class FlightModifierBitFlag
{
public:
    void SetFlag(EFlightModifierFlag flag)
    {
        m_modifierValue |= (int)flag;
    }

    void UnsetFlag(EFlightModifierFlag flag)
    {
        m_modifierValue &= ~(int)flag;
    }

    void ToggleFlag(EFlightModifierFlag flag)
    {
        m_modifierValue ^= (int)flag;
    }

    bool HasFlag(EFlightModifierFlag flag) const
    {
        return (m_modifierValue & (int)flag) == (int)flag;
    }
    bool HasAnyFlag(EFlightModifierFlag multiFlag) const
    {
        return (m_modifierValue & (int)multiFlag) != 0;
    }

private:
    uint8_t m_modifierValue = 0;
};
