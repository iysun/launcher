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

给 `:checked` 态的 `image:` 指定一个真实的图标资源，不要指望任何 QSS 伪元素/CSS 属性
能画出这个对勾。PNG 是 Qt 内置支持的格式，不需要额外链接 QtSvg 模块。

主题变成数据驱动、用户可任意新增（见 [AGENTS.md「自定义主题」](../../AGENTS.md)）之后，
没法再像早期那样为单一固定配色预烘焙一份 PNG 编进 `.qrc`。现在改为 `Theme::init()`
里的 `Theme::generateIcons()` 按当前主题色用 `QPainter` 现画，落盘到
`<datadir>/theme-cache/{check,chevron-down}.png`，QSS 用 `Theme::checkIconPath()` /
`Theme::chevronIconPath()` 返回的磁盘绝对路径通过 `image: url(...)` 引用——Qt 样式表
的 `url()` 支持磁盘绝对路径，不要求一定是 qrc 资源。同一颗心得依旧适用：下拉箭头
（`QComboBox::down-arrow`）在自定义了 combo box 背景/边框后同样会被 Qt 接管绘制，
不会回退到原生箭头，也需要一张真实图片。
