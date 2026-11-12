# Windows 10 上面板圆角失效、外侧还有方框痕迹

**现象**：`WindowOptions.CornerRadius` 设了也不圆；自绘圆角后，四角外侧仍能看到 1px 的方形边线和阴影。

**原因**：
- 圆角走 DWM 的 `DWMWA_WINDOW_CORNER_PREFERENCE`，只在 Windows 11 生效。
- Windows 10 上 `Vibrancy` 没有对应材质，但非空值会让 mygo 跳过 `plainTransparentFrame`，窗口继续保留 1px 边框和矩形阴影，圆角缺口处就会露出直角。

**做法**：Win10（build < 22000）上不设 `Vibrancy`、`DisableShadow`，由 `view.go` 自绘圆角卡片（`Radius + ClipInset`）；Win11 仍交给系统裁切与投影。
