#include "framelesswindow.h"
#include <QApplication>
#include <QPoint>
#include <QSize>
#include <windows.h>
#include <WinUser.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <objidl.h> // Fixes error C2504: 'IUnknown' : base class undefined
#include <QWindow>
#include <QtDebug>
#include <QResizeEvent>
#include <QScreen>
#include <memory>
#include <QTimer>

FramelessWindow::FramelessWindow(QWidget *parent)
    : QMainWindow(parent),
      m_titlebar(Q_NULLPTR),
      m_borderWidth(5),
      m_bResizeable(true)
{
    setWindowFlag(Qt::Window, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowSystemMenuHint, true);

    setResizeable(m_bResizeable);

    m_forceUpdateTimer.setSingleShot(true);
    m_forceUpdateTimer.setInterval(0);
    connect(&m_forceUpdateTimer, &QTimer::timeout, this, [this]() {
        const auto oldMask = mask();
        setMask(QRegion(this->rect()));
        setMask(oldMask);
    });
}

void FramelessWindow::setResizeable(bool resizeable)
{
    m_bResizeable = resizeable;
    HWND hwnd = reinterpret_cast<HWND>(this->effectiveWinId());
    if(hwnd == nullptr) {
        return;
    }

    if (m_bResizeable) {
        // this line will get titlebar/thick frame/Aero back, which is exactly what we want
        // we will get rid of titlebar and thick frame again in nativeEvent() later
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);

        // WS_CAPTION: 没有这项属性会导致在 VS 下出现窗口边框闪动(窗口激活状态切换时), 有这项属性会导致最大化时内容超出屏幕, 因此保留这项属性, 最大化时尺寸另作处理
        ::SetWindowLong(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_CAPTION | WS_THICKFRAME);
    } else {
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
        ::SetWindowLong(hwnd, GWL_STYLE, (style & ~WS_MAXIMIZEBOX) | WS_THICKFRAME);
    }

    // we better left 1 piexl width of border untouch, so OS can draw nice shadow around it
    const MARGINS shadow = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
}

bool FramelessWindow::isResizeable() const
{
    return m_bResizeable;
}

void FramelessWindow::setResizeableAreaWidth(int width)
{
    if (1 > width)
        width = 1;
    m_borderWidth = width;
}

void FramelessWindow::setTitleBar(QWidget* titlebar)
{
    m_titlebar = titlebar;
    if (!titlebar)
        return;
    connect(titlebar, SIGNAL(destroyed(QObject*)), this, SLOT(onTitleBarDestroyed()));
}

void FramelessWindow::onTitleBarDestroyed()
{
    if (m_titlebar == QObject::sender()) {
        m_titlebar = Q_NULLPTR;
    }
}

void FramelessWindow::addIgnoreWidget(QWidget* widget)
{
    if (!widget)
        return;
    if (m_whiteList.contains(widget))
        return;
    m_whiteList.append(widget);
}

QDebug operator<<(QDebug d, const RECT &r)
{
    QDebugStateSaver saver(d);
    d.nospace();
    d << "RECT(left=" << r.left << ", top=" << r.top
      << ", right=" << r.right << ", bottom=" << r.bottom
      << " (" << r.right - r.left << 'x' << r.bottom - r.top << "))";
    return d;
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    bool FramelessWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
#else
    bool FramelessWindow::nativeEvent(const QByteArray& eventType, void* message, long* result)
#endif
{
    // Workaround for known bug -> check Qt forum : https://forum.qt.io/topic/93141/qtablewidget-itemselectionchanged/13
#if (QT_VERSION == QT_VERSION_CHECK(5, 11, 1))
    MSG* msg = *static_cast<MSG**>(message);
#else
    MSG* msg = static_cast<MSG*>(message);
#endif

    switch (msg->message) {
        case WM_NCCALCSIZE: {
            // this kills the window frame and title bar we added with WS_THICKFRAME and WS_CAPTION
            NCCALCSIZE_PARAMS* sz = reinterpret_cast< NCCALCSIZE_PARAMS* >( msg->lParam );
            if(!::IsZoomed(msg->hwnd)) {
                // sz->rgrc[0] 的值必须跟原来的不同, 否则拉伸左/上边框缩放窗口时, 会导致右/下侧出现空白区域 (绘制抖动)
                // 窗口下边框失去1像素对视觉影响最小, 因此底部减少1像素
                sz->rgrc[ 0 ].bottom += 1;
            } else {
                // flags 包含 0x8000 时, 意味着可能出现了问题, 强制刷新窗口避免白屏, 微软的文档中找不到这个值的定义, 待研究
                if(sz->lppos->flags & 0x8000) {
                    m_forceUpdateTimer.start(); // 启动强制刷新定时器
                } else {
                    m_forceUpdateTimer.stop(); // 窗口已正常显示, 没有必要再执行强制刷新
                    // 修正最大化时内容超出屏幕问题
                    auto monitor = MonitorFromWindow(msg->hwnd, MONITOR_DEFAULTTONEAREST);
                    MONITORINFO info;
                    info.cbSize = sizeof(MONITORINFO);
                    if(GetMonitorInfo(monitor, &info)) {
                        const auto workRect = info.rcWork;
                        sz->rgrc[0].left = qMax(sz->rgrc[0].left, long(workRect.left));
                        sz->rgrc[0].top = qMax(sz->rgrc[0].top, long(workRect.top));
                        sz->rgrc[0].right = qMin(sz->rgrc[0].right, long(workRect.right));
                        sz->rgrc[0].bottom = qMin(sz->rgrc[0].bottom, long(workRect.bottom));
                    }
                }
            }

            *result = 0;
            return true;
        }
        case WM_NCHITTEST: {
            *result = 0;

            const LONG border_width = m_borderWidth;
            RECT winrect{};
            GetWindowRect(reinterpret_cast<HWND>(this->winId()), &winrect);

            const long x = GET_X_LPARAM(msg->lParam);
            const long y = GET_Y_LPARAM(msg->lParam);

            if (m_bResizeable) {
                const bool resizeWidth = minimumWidth() != maximumWidth();
                const bool resizeHeight = minimumHeight() != maximumHeight();

                if (resizeWidth) {
                    // left border
                    if (x >= winrect.left && x < winrect.left + border_width) {
                        *result = HTLEFT;
                    }
                    // right border
                    if (x < winrect.right && x >= winrect.right - border_width) {
                        *result = HTRIGHT;
                    }
                }
                if (resizeHeight) {
                    // bottom border
                    if (y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOM;
                    }
                    // top border
                    if (y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOP;
                    }
                }
                if (resizeWidth && resizeHeight) {
                    // bottom left corner
                    if (x >= winrect.left && x < winrect.left + border_width && y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOMLEFT;
                    }
                    // bottom right corner
                    if (x < winrect.right && x >= winrect.right - border_width && y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOMRIGHT;
                    }
                    // top left corner
                    if (x >= winrect.left && x < winrect.left + border_width && y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOPLEFT;
                    }
                    // top right corner
                    if (x < winrect.right && x >= winrect.right - border_width && y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOPRIGHT;
                    }
                }
            }
            if (0 != *result)
                return true;

            //*result still equals 0, that means the cursor locate OUTSIDE the frame area
            // but it may locate in titlebar area
            if (!m_titlebar)
                return false;

            // support highdpi
            const double dpr = this->devicePixelRatioF();
            const QPoint pos = m_titlebar->mapFromGlobal(QPoint(x / dpr, y / dpr));

            if (!m_titlebar->rect().contains(pos))
                return false;
            QWidget* child = m_titlebar->childAt(pos);
            if (!child) {
                *result = HTCAPTION;
                return true;
            } else {
                if (m_whiteList.contains(child)) {
                    *result = HTCAPTION;
                    return true;
                }
            }
            return false;
        } // end case WM_NCHITTEST
        case WM_WINDOWPOSCHANGING: {
            // Tell Windows to discard the entire contents of the client area, as re-using
            // parts of the client area would lead to jitter during resize.
            auto* windowPos = reinterpret_cast<WINDOWPOS*>(msg->lParam);
            windowPos->flags |= SWP_NOCOPYBITS;
            break;
        }
        default:
            break;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

bool FramelessWindow::event(QEvent* event)
{
    switch (event->type()) {
        case QEvent::WindowStateChange: {
            // 在窗口最大化时, 用鼠标向下拖拽标题栏还原窗口, 不松手然后重新贴边最大化, 此时再进行窗口还原时(包括双击标题栏, showNormal()等方式), 标题栏会有一部分在屏幕之外
            // 这种现象无论有没有使用无边框属性都会发生, 应该是 Qt 的又一个 Bug (测试场景 Qt 5.15/Qt 6.3 + Win10)
            // 此处进行行为修正
            if(windowState() == Qt::WindowNoState) {
                const auto workRect = qApp->primaryScreen()->availableVirtualGeometry();
                // 这个地方不能直接用 pos() 方法, 因为 Qt 的尺寸和位置相关接口总是延迟几帧, 此时拿到的 pos() 仍然是最大化时的位置
                RECT rect;
                GetWindowRect(reinterpret_cast<HWND>(this->winId()), &rect);

                if(rect.top < workRect.top()) {
                    move(rect.left, workRect.top());
                }
            }
            break;
        }
        case QEvent::WinIdChange:
            setResizeable(m_bResizeable);
            break;
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        case QEvent::ScreenChangeInternal: {
            // 通过设置Mask强制触发更新, 修正跨屏拖拽时的错位问题, 同时会导致失去窗口阴影
            const auto oldMask = mask();
            setMask(QRegion(this->rect()));
            setMask(oldMask);

            // 重新设置窗口属性, 把窗口阴影带回来
            setResizeable(m_bResizeable);
            break;
        }
#endif
        default:
            break;
    }
    return QMainWindow::event(event);
}
