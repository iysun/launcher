#pragma once
#include <QByteArray>
#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <memory>

struct VTerm;       // 前置声明，避免在头文件泄漏 libvterm
struct VTermScreen; // （实际类型是 typedef struct，.cpp 里 include vterm.h）

// libvterm 内核封装：喂 PTY 字节 → 维护屏幕网格；把键盘编码成 VT 字节。
// 唯一 include <vterm.h> 的地方，便于跟随其 API 演进。**只在 GUI 线程访问**。
class TerminalCore : public QObject {
    Q_OBJECT
public:
    // 供 TerminalView 绘制的单元格快照（不含 libvterm 类型）
    struct Cell {
        QString text;      // 组合后的字形；空串表示空白格
        QColor  fg, bg;    // 已解析成 RGB
        int     width = 1; // 1 或 2（宽字符）
        bool    bold = false, underline = false, italic = false, reverse = false;
    };

    // 与 Qt::Key 解耦的特殊键
    enum class SpecialKey {
        Enter, Tab, Backspace, Escape,
        Up, Down, Left, Right,
        Insert, Delete, Home, End, PageUp, PageDown,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12
    };

    // 应用请求的鼠标追踪模式（VTERM_PROP_MOUSE 映射）：None=应用不要鼠标，
    // 视图自行做本地选区；Click=仅按下/释放；Drag=按下期间的移动也上报；
    // Move=任意移动都上报（需 setMouseTracking）。
    enum class MouseMode { None, Click, Drag, Move };

    explicit TerminalCore(int rows, int cols, QObject *parent = nullptr);
    ~TerminalCore() override;

    void feed(const QByteArray &bytes); // ← PTY 输出：vterm_input_write
    void setSize(int rows, int cols);   // vterm_set_size
    void keyChar(uint32_t codepoint, Qt::KeyboardModifiers mods);
    void keySpecial(SpecialKey key, Qt::KeyboardModifiers mods);
    void pasteText(const QString &text); // bracketed paste（应用开启该模式时自动包裹）

    // ── 鼠标上报（vim/htop/tmux 鼠标模式）：编码字节经 outputToPty 写回 PTY ──
    MouseMode mouseMode() const { return m_mouseMode; }
    void      mouseMove(int row, int col, Qt::KeyboardModifiers mods);   // 0-based 屏幕格
    void      mouseButton(int button, bool pressed, Qt::KeyboardModifiers mods); // 1=左 2=中 3=右 4/5=滚轮

    // ANSI 16 色调色板（索引 0..15）注入 vterm，与主题联动；启动时调一次。
    void applyAnsiPalette(const QList<QColor> &palette);

    Cell cellAt(int row, int col) const; // vterm_screen_get_cell + 颜色转 RGB
    int  rows() const { return m_rows; }
    int  cols() const { return m_cols; }

    // ── scrollback（滚动回看）：历史行存储在 .cpp 内（含 libvterm 类型），只暴露快照
    int    historySize() const;                       // 当前缓存的历史行数
    qint64 historyStart() const;                      // 已因容量上限驱逐的行数（全局行号基准）
    Cell   historyCellAt(int histRow, int col) const; // histRow ∈ [0, historySize)，0 = 最旧
    bool   altScreen() const { return m_altScreen; }  // 备用屏（vim/htop）下无 scrollback

    int  cursorRow() const { return m_cursorRow; }
    int  cursorCol() const { return m_cursorCol; }
    bool cursorVisible() const { return m_cursorVisible; }

    // 设默认前景/背景（与 app 主题联动）
    void setDefaultColors(const QColor &fg, const QColor &bg);

    // ── 内部：由 .cpp 里的 libvterm 回调蹦床调用（user == this）。逻辑上私有，
    //    因需从文件内 static 回调触达且要 emit 信号，故置为 public。外部勿直接调用。
    void sinkOutput(const char *s, int len);
    void sinkDamage(int row, int col, int height, int width);
    void sinkCursor(int row, int col, bool visible);
    void sinkBell();
    void sinkTitleFragment(const char *s, int len, bool initial, bool final);
    void sinkPushLine(int cols, const void *cells); // cells: const VTermScreenCell*
    int  sinkPopLine(int cols, void *cells);        // cells: VTermScreenCell*（回填）
    void sinkSbClear();
    void sinkAltScreen(bool on);
    void sinkMouseMode(int vtermMouseProp); // VTERM_PROP_MOUSE_* → MouseMode

signals:
    void outputToPty(const QByteArray &bytes);          // 键盘编码 / 查询响应 → 写回 PTY
    void damaged(int row, int col, int height, int width); // 脏区（行列 + 高宽，单位：格）
    void cursorMoved(int row, int col, bool visible);
    void bell();
    void titleChanged(const QString &title);
    void scrollbackChanged(int delta); // 历史行数变化：+1 push / -1 pop / -n clear
    void mouseModeChanged(MouseMode mode); // 应用切换鼠标追踪模式

private:
    struct Scrollback; // 定义在 .cpp（内含 VTermScreenCell，不泄漏到头文件）

    VTerm       *m_vt     = nullptr;
    VTermScreen *m_screen = nullptr;
    int          m_rows = 0, m_cols = 0;
    int          m_cursorRow = 0, m_cursorCol = 0;
    bool         m_cursorVisible = true;
    bool         m_altScreen = false;
    MouseMode    m_mouseMode = MouseMode::None;
    QString      m_titleBuf;
    std::unique_ptr<Scrollback> m_sb;
};
