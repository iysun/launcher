# 终端 Ctrl+[ 不能直接交给 libvterm 编码

## 现象

想让内置终端支持 `Ctrl+[`（vim 用户日常，等价 Esc）、`Ctrl+\`、`Ctrl+]`、`Ctrl+Space` 等控制键。
直觉做法是照搬现成的 `Ctrl+字母` 分支——把基字符 + `CTRL` 修饰交给 `TerminalCore::keyChar`
（内部 `vterm_keyboard_unichar`），指望 libvterm 做 `c & 0x1f` 生成控制字节。

但 `Ctrl+[` 这么发出去，vim 收到的不是 `0x1B`（Esc），而是 `ESC[91;5u`——一段
**CSI u**（fixterms/kitty 键盘协议）序列。vanilla vim / 多数 TUI 不认，表现为 `Ctrl+[`
无法退出插入模式、按键像被吞掉。

## 原因

libvterm 0.3.3 `src/keyboard.c` 的 `vterm_keyboard_unichar` 对部分字符**强制走 CSI u**
以消除歧义：

```c
switch(c) {
  /* 这些 Ctrl- 字符无法用别的方式表达（Ctrl+I=Tab、Ctrl+M=Enter、Ctrl+[=Esc 会和
     真正的 Tab/Enter/Escape 键混淆），故用 CSI u 区分 */
  case 'i': case 'j': case 'm': case '[':
    needs_CSIu = 1; break;
  /* Ctrl+\ ] ^ _ 不会和别的键冲突，不需要 CSI u */
  case '\\': case ']': case '^': case '_':
    needs_CSIu = 0; break;
  ...
}
if(needs_CSIu && (mod & ~VTERM_MOD_ALT)) {   // 带 CTRL 就命中
  ...CSI u...; return;                         // ← Ctrl+[ 在这里被编码成 ESC[91;5u
}
if(mod & VTERM_MOD_CTRL) c &= 0x1f;          // ← 只有 needs_CSIu==0 的才走到这
```

所以 `\ ] ^ _` 和 `Space`（`' ' & 0x1f == 0x00`）交给 `keyChar` 是对的，会得到
`0x1C/0x1D/0x1E/0x1F/0x00`；而 `[`（以及 `i`/`j`/`m`）会被 CSI u 截走。
注意 `keySpecial(Escape, CTRL)` 同样中招——`KEYCODE_LITERAL` 分支里
`if(mod & (SHIFT|CTRL))` 也走 CSI u（`ESC[27;5u`）。

## 正确做法（见 `src/ui/terminalview.cpp` keyPressEvent）

- `Ctrl+[` → 直接发**无修饰**的 Escape：`keySpecial(SpecialKey::Escape, Qt::NoModifier)`，
  得到裸 `0x1B`，正是用户要的「Ctrl+[ == Esc」。
- `Ctrl+\ ] ^ _` 和 `Ctrl+Space` → `keyChar(base, Qt::ControlModifier)`，让 libvterm 的
  `c & 0x1f` 正确生成控制字节。只传 `ControlModifier`（不透传原始 mods），避免 Shift 混进来
  又触发 CSI u。

一句话：**凡是 libvterm 里 `needs_CSIu==1` 的键，别指望 `keyChar` 给你控制字节**，要么绕开
（如 `[`→Escape 特例），要么接受它就是 CSI u。改键盘处理时先翻一眼 `keyboard.c` 的那张
`switch`。
