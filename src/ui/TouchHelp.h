// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "ui/UiProfile.h"
#include "core/UiStrings.h"

namespace Ui::TouchHelp {
inline const char* title(int page) {
#if STICKMON_UI_ROUND_TOUCH
    if (page == 0) return "触摸操作";
#endif
    return Ui::Settings::HELP_PAGE_TITLES[page];
}
inline const char* line(int page, int row) {
#if STICKMON_UI_ROUND_TOUCH
    static constexpr const char* lines[][4] = {
        {"按屏幕操作", "点击确认选项", "点击左上角返回", "上下滑动长列表"},
        {"点击精灵互动", "点击食盆添加食物", "点击底部菜单", "进度会自动保存"},
        {"点击条目进入", "上下滑动查看", "状态页左右滑动", "返回键回到上一层"},
        {"点区域切换预览", "再点当前区域出发", "点击地图开始前进", "右下角打开探索菜单"},
        {"洗澡右侧选择工具", "按住精灵拖动擦洗", "松手即停止擦洗", "短按电源锁屏恢复"},
        {"重播房间和菜单提示", "重播探索和战斗提示", "点击重新引导", "返回键回到设置"},
    };
    return lines[page][row];
#else
    return Ui::Settings::TOUCH_HELP_PAGE_LINES[page][row];
#endif
}
}  // namespace Ui::TouchHelp
