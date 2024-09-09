#pragma once

#include <functional>
#include <string>

typedef std::function<bool(int iSlot, const char* szWeapon)> OnWeaponRestrictedCallback;

#define RW_INTERFACE "IRWApi"
class IRWApi
{
public:
    virtual void HookOnWeaponRestricted(OnWeaponRestrictedCallback callback) = 0;
};