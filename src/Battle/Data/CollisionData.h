#pragma once
#include <cstdint>

#include "CollisionBox.h"
#include "CString.h"

#pragma pack (push, 1)
struct CollisionData
{
    int32_t Magic = 0x434F4C00; //COL
    CString<64> Name;
    int16_t BoxTypeCount = static_cast<int16_t>(BoxType::Count);
    int16_t HurtCount = 0;
    int16_t HitCount = 0;
    CollisionBox* Boxes = nullptr;
};

#pragma pack(pop)