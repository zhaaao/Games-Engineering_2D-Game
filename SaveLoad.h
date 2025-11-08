#pragma once
#include "Player.h"
#include "NPCSystem.h"

// 一个极简的二进制存档接口。
// 保存：模式(是否无限)、玩家位置/武器参数、总时间/击杀、NPC快照(type/x/y/fireCD)
// 读取：重建上述状态。NPC的 hp/w/h 无公共setter，按“类型默认参数”重建（见实现说明）。

namespace SaveLoad
{
    // 返回 true 表示成功
    bool SaveToFile(const char* path,
        const Player& hero,
        const EnemyManager& npcs,
        float totalTime,
        int totalKills,
        bool infiniteMode);

    // 读取后：会清空并重建 NPC；设置 NPCSystem 的 infinite 标志；
    // 调用者需要据此设置 TileMap 的 wrap（见 main.cpp 示例）。
    bool LoadFromFile(const char* path,
        Player& hero,
        EnemyManager& npcs,
        float& totalTime,
        int& totalKills,
        bool& infiniteMode);
}
