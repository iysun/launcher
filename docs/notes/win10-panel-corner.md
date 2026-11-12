# Windows 10 上面板没有圆角

**现象**：`WindowOptions.CornerRadius` 设为 16，卡片四角仍是直角。

**原因**：mygo 的圆角走 DWM 的 `DWMWA_WINDOW_CORNER_PREFERENCE`，只在 Windows 11 生效；Windows 10 也没有 vibrancy 材质，`ui.Context.Vibrancy()` 为 false。

**做法**：`Vibrancy()` 为 false 时自己画圆角卡片（`root.Radius+ClipInset`），有材质时仍交给系统裁切。
