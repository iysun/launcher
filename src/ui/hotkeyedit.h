#pragma once
#include <QKeySequence>
#include <QLineEdit>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// 快捷键录制控件。
// 点击后进入录制模式，通过 WH_KEYBOARD_LL 低级钩子拦截按键，
// 使其能捕获 Alt+Space 等系统保留组合键（普通 QKeySequenceEdit 无法做到）。
class HotkeyEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit HotkeyEdit(QWidget *parent = nullptr);
    ~HotkeyEdit();

    void         setKeySequence(const QKeySequence &seq);
    QKeySequence keySequence() const { return m_sequence; }

protected:
    void mousePressEvent(QMouseEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
#ifndef Q_OS_WIN
    void keyPressEvent(QKeyEvent *e) override;
#endif

private:
    void startRecording();
    void stopRecording();
    void onKeyCaptured(int qtKey, Qt::KeyboardModifiers mods);
    void updateDisplay();

#ifdef Q_OS_WIN
    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);
    static HotkeyEdit      *s_instance;
    HHOOK                   m_hook = nullptr;
#endif

    QKeySequence m_sequence;
    bool         m_recording = false;
};
