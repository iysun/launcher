# Qt 样式表不支持 ::after / content，勾选框对勾需要自己画

## 现象 / 背景

`SettingsDialog` 的 `checkStyle` 曾经这样给选中态画对勾：

```css
QCheckBox::indicator:checked {
    background: %4;
    border-color: %4;
    image: none;
}
QCheckBox::indicator:checked:after {
    content: "✓";
}
```

`::after` + `content:` 完全不生效——Qt 样式表（QSS）不是浏览器 CSS，不支持这套写法，
Qt 会直接忽略这条规则。选中态看起来只是一个纯色方块，用户分不清是否勾选。

## 原因

一旦 `QCheckBox::indicator` 的基础规则（unchecked 态）自定义了 `background`/`border`，
Qt 的 `QStyleSheetStyle` 就整体接管了这个子控件的绘制，**不会再回退到原生对勾**——
所以哪怕去掉 `image: none;`，原生对勾也不会自动出现，必须自己提供对勾图。

## 正确做法

给 `:checked` 态的 `image:` 指定一个真实的图标资源（`image: url(:/icons/check.png)`），
不要指望任何 QSS 伪元素/CSS 属性能画出这个对勾。图标由 `tools/gen_check_icon.py`
生成（Pillow 画一个小对勾，输出到 `resources/icons/check.png`），改颜色/尺寸就重新
跑一遍脚本，不要手工改产物。PNG 是 Qt 内置支持的格式，不需要额外链接 QtSvg 模块。
