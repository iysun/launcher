#include "hotkeyedit.h"
#include "core/i18n.h"
#include <QFocusEvent>
#include <QKeyEvent>
#include <QMouseEvent>

// ── Windows 实现（低级键盘钩子） ──────────────────────────────────

#ifdef Q_OS_WIN

HotkeyEdit *HotkeyEdit::s_instance = nullptr;

// VK 码 → Qt 键值（仅覆盖全局快捷键场景中的常用键）
static int vkToQtKey(DWORD vk) {
    if (vk == VK_SPACE) return Qt::Key_Space;
    if (vk >= 0x41 && vk <= 0x5A) return Qt::Key_A + (vk - 0x41); // A–Z
    if (vk >= 0x30 && vk <= 0x39) return Qt::Key_0 + (vk - 0x30); // 0–9
    if (vk >= VK_F1 && vk <= VK_F12) return Qt::Key_F1 + (vk - VK_F1);
    switch (vk) {
    case VK_TAB:
        return Qt::Key_Tab;
    case VK_RETURN:
        return Qt::Key_Return;
    case VK_ESCAPE:
        return Qt::Key_Escape;
    case VK_HOME:
        return Qt::Key_Home;
    case VK_END:
        return Qt::Key_End;
    case VK_LEFT:
        return Qt::Key_Left;
    case VK_RIGHT:
        return Qt::Key_Right;
    case VK_UP:
        return Qt::Key_Up;
    case VK_DOWN:
        return Qt::Key_Down;
    case VK_DELETE:
        return Qt::Key_Delete;
    case VK_INSERT:
        return Qt::Key_Insert;
    case VK_BACK:
        return Qt::Key_Backspace;
    case VK_OEM_MINUS:
        return Qt::Key_Minus;
    case VK_OEM_PLUS:
        return Qt::Key_Equal;
    case VK_OEM_COMMA:
        return Qt::Key_Comma;
    case VK_OEM_PERIOD:
        return Qt::Key_Period;
    case VK_OEM_1:
        return Qt::Key_Semicolon;
    case VK_OEM_2:
        return Qt::Key_Slash;
    case VK_OEM_3:
        return Qt::Key_QuoteLeft;
    case VK_OEM_4:
        return Qt::Key_BracketLeft;
    case VK_OEM_5:
        return Qt::Key_Backslash;
    case VK_OEM_6:
        return Qt::Key_BracketRight;
    case VK_OEM_7:
        return Qt::Key_Apostrophe;
    default:
        return 0;
    }
}

// 钩子在安装线程（主线程）消息泵中被调用，可安全访问主线程对象
LRESULT CALLBACK HotkeyEdit::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance && s_instance->m_recording) {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            auto       *kb = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
            const DWORD vk = kb->vkCode;

            // 跳过纯修饰键，等待用户按下主键
            if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL ||
                vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_MENU ||
                vk == VK_LMENU || vk == VK_RMENU || vk == VK_LWIN || vk == VK_RWIN)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            Qt::KeyboardModifiers mods = Qt::NoModifier;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= Qt::ControlModifier;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= Qt::ShiftModifier;
            if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= Qt::AltModifier;
            if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000)
                mods |= Qt::MetaModifier;

            const int qtKey = vkToQtKey(vk);
            if (qtKey != 0) {
                s_instance->onKeyCaptured(qtKey, mods);
                return 1; // 拦截：不再传递给系统，避免触发系统菜单等副作用
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void HotkeyEdit::startRecording() {
    if (m_recording) return;
    m_recording = true;
    s_instance  = this;
    m_hook      = SetWindowsHookEx(WH_KEYBOARD_LL, hookProc, GetModuleHandle(nullptr), 0);
    setText(I18n::t("hotkey.pressKeys"));
    setReadOnly(true);
}

void HotkeyEdit::stopRecording() {
    if (!m_recording) return;
    m_recording = false;
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
    s_instance = nullptr;
    setReadOnly(false);
    updateDisplay();
}

#else
// ── 非 Windows：普通键盘事件捕获（不支持系统保留键） ─────────────

void HotkeyEdit::startRecording() {
    m_recording = true;
    setText(I18n::t("hotkey.pressKeys"));
}

void HotkeyEdit::stopRecording() {
    m_recording = false;
    updateDisplay();
}

void HotkeyEdit::keyPressEvent(QKeyEvent *e) {
    if (!m_recording) {
        QLineEdit::keyPressEvent(e);
        return;
    }
    const int key = e->key();
    if (key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt ||
        key == Qt::Key_Meta)
        return;
    onKeyCaptured(key, e->modifiers());
}

#endif

// ── 公共实现 ───────────────────────────────────────────────────

HotkeyEdit::HotkeyEdit(QWidget *parent) : QLineEdit(parent) {
    setPlaceholderText(I18n::t("hotkey.clickToRecord"));
    setReadOnly(false);
    updateDisplay();
}

HotkeyEdit::~HotkeyEdit() {
    stopRecording();
}

void HotkeyEdit::setKeySequence(const QKeySequence &seq) {
    m_sequence = seq;
    updateDisplay();
}

void HotkeyEdit::onKeyCaptured(int qtKey, Qt::KeyboardModifiers mods) {
    m_sequence = QKeySequence(static_cast<int>(mods) | qtKey);
    stopRecording();
}

void HotkeyEdit::updateDisplay() {
    setText(m_sequence.isEmpty() ? QString()
                                 : m_sequence.toString(QKeySequence::NativeText));
}

void HotkeyEdit::mousePressEvent(QMouseEvent *e) {
    if (!m_recording) startRecording();
    QLineEdit::mousePressEvent(e);
}

void HotkeyEdit::focusOutEvent(QFocusEvent *e) {
    stopRecording();
    QLineEdit::focusOutEvent(e);
}
