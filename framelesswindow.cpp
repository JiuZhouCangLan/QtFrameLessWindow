#include "framelesswindow.h"
#include <QApplication>
#include <QPoint>
#include <QSize>
#include <windows.h>
#include <WinUser.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <objidl.h> // Fixes error C2504: 'IUnknown' : base class undefined
#include <gdiplus.h>
#include <QWindow>
#include <QtDebug>
#include <QResizeEvent>
#include <QScreen>
#include <memory>
#include <QTimer>

#pragma warning(disable : 26434)
#pragma comment(lib, "Dwmapi.lib") // Adds missing library, fixes error LNK2019: unresolved external symbol __imp__DwmExtendFrameIntoClientArea
#pragma comment(lib, "user32.lib")

CFramelessWindow::CFramelessWindow(QWidget *parent)
    : QMainWindow(parent),
      m_titlebar(Q_NULLPTR),
      m_borderWidth(5),
      m_bJustMaximized(false),
      m_bResizeable(true)
{
    setWindowFlag(Qt::Window, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowSystemMenuHint, true);

    setResizeable(m_bResizeable);
}

void CFramelessWindow::setResizeable(bool resizeable)
{
    const bool visible = isVisible();
    m_bResizeable = resizeable;
    if (m_bResizeable) {
        //this line will get titlebar/thick frame/Aero back, which is exactly what we want
        //we will get rid of titlebar and thick frame again in nativeEvent() later
        HWND hwnd = HWND(this->winId());
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
        ::SetWindowLong(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_POPUP | WS_THICKFRAME);
    } else {
        HWND hwnd = HWND(this->winId());
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
        ::SetWindowLong(hwnd, GWL_STYLE, (style & ~WS_MAXIMIZEBOX & ~WS_CAPTION) | WS_THICKFRAME);
    }

    //we better left 1 piexl width of border untouch, so OS can draw nice shadow around it
    const MARGINS shadow = { 1, 1, 1, 1 };
    DwmExtendFrameIntoClientArea(HWND(winId()), &shadow);

    setVisible(visible);
}

void CFramelessWindow::setResizeableAreaWidth(int width)
{
    if (1 > width) width = 1;
    m_borderWidth = width;
}

void CFramelessWindow::setTitleBar(QWidget* titlebar)
{
    m_titlebar = titlebar;
    if (!titlebar) return;
    connect(titlebar, SIGNAL(destroyed(QObject*)), this, SLOT(onTitleBarDestroyed()));
}

void CFramelessWindow::onTitleBarDestroyed()
{
    if (m_titlebar == QObject::sender()) {
        m_titlebar = Q_NULLPTR;
    }
}

void CFramelessWindow::addIgnoreWidget(QWidget* widget)
{
    if (!widget) return;
    if (m_whiteList.contains(widget)) return;
    m_whiteList.append(widget);
}

#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    bool CFramelessWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
#else
    bool CFramelessWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
#endif
{
    //Workaround for known bug -> check Qt forum : https://forum.qt.io/topic/93141/qtablewidget-itemselectionchanged/13
#if (QT_VERSION == QT_VERSION_CHECK(5, 11, 1))
    MSG* msg = *static_cast<MSG**>(message);
#else
    MSG* msg = static_cast<MSG*>(message);
#endif

    switch (msg->message) {
        case WM_NCCALCSIZE: {
            NCCALCSIZE_PARAMS& params = *reinterpret_cast<NCCALCSIZE_PARAMS*>(msg->lParam);
            if (params.rgrc[0].top != 0)
                params.rgrc[0].top -= 1;
            QMainWindow::nativeEvent(eventType, message, result);
            //this kills the window frame and title bar we added with WS_THICKFRAME and WS_CAPTION
            *result = WVR_REDRAW;
            return true;
        }
        case WM_NCHITTEST: {
            *result = 0;

            const LONG border_width = m_borderWidth;
            RECT winrect;
            GetWindowRect(HWND(winId()), &winrect);

            const long x = GET_X_LPARAM(msg->lParam);
            const long y = GET_Y_LPARAM(msg->lParam);

            if(m_bResizeable) {
                const bool resizeWidth = minimumWidth() != maximumWidth();
                const bool resizeHeight = minimumHeight() != maximumHeight();

                if(resizeWidth) {
                    //left border
                    if (x >= winrect.left && x < winrect.left + border_width) {
                        *result = HTLEFT;
                    }
                    //right border
                    if (x < winrect.right && x >= winrect.right - border_width) {
                        *result = HTRIGHT;
                    }
                }
                if(resizeHeight) {
                    //bottom border
                    if (y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOM;
                    }
                    //top border
                    if (y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOP;
                    }
                }
                if(resizeWidth && resizeHeight) {
                    //bottom left corner
                    if (x >= winrect.left && x < winrect.left + border_width &&
                            y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOMLEFT;
                    }
                    //bottom right corner
                    if (x < winrect.right && x >= winrect.right - border_width &&
                            y < winrect.bottom && y >= winrect.bottom - border_width) {
                        *result = HTBOTTOMRIGHT;
                    }
                    //top left corner
                    if (x >= winrect.left && x < winrect.left + border_width &&
                            y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOPLEFT;
                    }
                    //top right corner
                    if (x < winrect.right && x >= winrect.right - border_width &&
                            y >= winrect.top && y < winrect.top + border_width) {
                        *result = HTTOPRIGHT;
                    }
                }
            }
            if (0 != *result) return true;

            //*result still equals 0, that means the cursor locate OUTSIDE the frame area
            //but it may locate in titlebar area
            if (!m_titlebar) return false;

            //support highdpi
            const double dpr = this->devicePixelRatioF();
            const QPoint pos = m_titlebar->mapFromGlobal(QPoint(x / dpr, y / dpr));

            if (!m_titlebar->rect().contains(pos)) return false;
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
        } //end case WM_NCHITTEST
        case WM_GETMINMAXINFO: {
            if (::IsZoomed(msg->hwnd)) {
                RECT frame = { 0, 0, 0, 0 };
                AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);

                //record frame area data
                const double dpr = this->devicePixelRatioF();

                m_frames.setLeft(abs(frame.left) / dpr + 0.5);
                m_frames.setTop(abs(frame.bottom) / dpr + 0.5);
                m_frames.setRight(abs(frame.right) / dpr + 0.5);
                m_frames.setBottom(abs(frame.bottom) / dpr + 0.5);

                m_bJustMaximized = true;
            } else {
                if (m_bJustMaximized) {
                    QMainWindow::setContentsMargins(m_margins);
                    m_bJustMaximized = false;
                    m_justNormaled = true;
                }
            }
            return false;
        }
        default:
            return QMainWindow::nativeEvent(eventType, message, result);
    }
}

void CFramelessWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);

    if (m_bJustMaximized) {
        HWND hwnd = (HWND)this->winId();
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        std::unique_ptr<MONITORINFO> monitorInfo(new MONITORINFO);
        monitorInfo->cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, monitorInfo.get());
        const auto workRect = monitorInfo->rcWork;

        // 补偿阴影尺寸计算带来的边缘超出边界问题
        if(event->size().width() > workRect.right - workRect.left) {
            QMainWindow::setContentsMargins(m_frames + m_margins);
        }

        // 修正在副屏上最大化会盖掉状态栏问题
        if(workRect.bottom - workRect.top < event->size().height() - m_frames.top() - m_frames.bottom()) {
            resize(event->size().width(), workRect.bottom - workRect.top + m_frames.top() + m_frames.bottom());
        }
    } else if(m_justNormaled) {
        m_frames = QMargins();
        QMainWindow::setContentsMargins(m_margins);
    }
}

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
bool CFramelessWindow::event(QEvent *event)
{
    if(event->type() == QEvent::ScreenChangeInternal) {
        // 通过设置Mask强制触发更新, 修正双屏拖拽时的错位问题, 同时会导致失去窗口阴影
        const auto oldMask = mask();
        setMask(QRegion(this->rect()));
        setMask(oldMask);

        // 重新设置窗口属性, 把窗口阴影带回来
        setResizeable(m_bResizeable);
    }

    return QMainWindow::event(event);
}
#endif

void CFramelessWindow::setContentsMargins(const QMargins &margins)
{
    QMainWindow::setContentsMargins(margins + m_frames);
    m_margins = margins;
}

void CFramelessWindow::setContentsMargins(int left, int top, int right, int bottom)
{
    QMainWindow::setContentsMargins(left + m_frames.left(), \
                                    top + m_frames.top(), \
                                    right + m_frames.right(), \
                                    bottom + m_frames.bottom());
    m_margins.setLeft(left);
    m_margins.setTop(top);
    m_margins.setRight(right);
    m_margins.setBottom(bottom);
}

QMargins CFramelessWindow::contentsMargins() const
{
    QMargins margins = QMainWindow::contentsMargins();
    margins -= m_frames;
    return margins;
}

QRect CFramelessWindow::contentsRect() const
{
    QRect rect = QMainWindow::contentsRect();
    const int width = rect.width();
    const int height = rect.height();
    rect.setLeft(rect.left() - m_frames.left());
    rect.setTop(rect.top() - m_frames.top());
    rect.setWidth(width);
    rect.setHeight(height);
    return rect;
}
