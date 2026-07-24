texts = {
    "title": "// 系统启动中...",
    "skip": "[ 按 Space 跳过 ]",
    "start": "[ 按 Space 开始游戏 ]",
    "paused": "已暂停",
    "score_label": "分数",
    "time_label": "时间",
    "key_jump": "W - 跳跃",
    "key_move": "A/D - 移动",
    "key_hook": "鼠标左键 - 钩锁",
    "key_reset": "R - 重置",
    "key_resume": "空格 - 继续",
    "victory": "通关成功",
    "press_r": "按 R 重新开始",
    "teleport": "传送核心已激活",
}
for name, text in texts.items():
    escaped = "".join(f"\\u{ord(c):04x}" if ord(c) > 127 else c for c in text)
    print(f"{name}: {escaped}")
print("---DONE---")
