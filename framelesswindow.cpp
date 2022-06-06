﻿#include "framelesswindow.h"
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
      m_bJustMaximized(false),
      m_bResizeable(true)
{
    setWindowFlag(Qt::Window, true);
    setWindowFlag(Qt::FramelessWindowHint, true);
    setWindowFlag(Qt::WindowSystemMenuHint, true);

    setResizeable(m_bResizeable);
}

void FramelessWindow::setResizeable(bool resizeable)
{
    const bool visible = isVisible();
    m_bResizeable = resizeable;
    HWND hwnd = reinterpret_cast<HWND>(this->winId());
    if (m_bResizeable) {
        // this line will get titlebar/thick frame/Aero back, which is exactly what we want
        // we will get rid of titlebar and thick frame again in nativeEvent() later
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);

        // WS_CAPTION: 没有这项属性会导致在 VS 下出现窗口边框闪动(切换当前窗口时), 有这项属性会导致最大化时边缘超出屏幕范围, 因此保留这项属性, 最大化时尺寸另作处理
        ::SetWindowLong(hwnd, GWL_STYLE, style | WS_MAXIMIZEBOX | WS_CAPTION | WS_THICKFRAME);
    } else {
        const DWORD style = ::GetWindowLong(hwnd, GWL_STYLE);
        ::SetWindowLong(hwnd, GWL_STYLE, (style & ~WS_MAXIMIZEBOX) | WS_THICKFRAME);
    }

    // we better left 1 piexl width of border untouch, so OS can draw nice shadow around it
    const MARGINS shadow = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &shadow);

    setVisible(visible);
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
            QMainWindow::nativeEvent(eventType, message, result);
            // this kills the window frame and title bar we added with WS_THICKFRAME and WS_CAPTION
            *result = WVR_REDRAW;
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
        case WM_GETMINMAXINFO: {
            if (::IsZoomed(msg->hwnd)) {
                // 通过拖拽标题栏到屏幕上边界执行窗口最大化时, 如果窗口有一部分在显示区域之外, 大概率会导致最大化之后, 窗口边缘超出屏幕, 因此在此之前, 将窗口拉回显示区域
                if(m_windowMoving) {
                    RECT windowRect{};
                    GetWindowRect(msg->hwnd, &windowRect);

                    const auto workRect = qApp->primaryScreen()->availableVirtualGeometry();

                    const QPoint currentPos(windowRect.left, windowRect.top);
                    QPoint targetPos = currentPos;

                    // 左越界
                    if(workRect.left() > currentPos.x()) {
                        targetPos.setX(workRect.left());
                    }
                    // 右越界
                    if(workRect.right() < windowRect.right) {
                        targetPos.setX(workRect.right() - windowRect.right + windowRect.left);
                    }

                    if(currentPos != targetPos) {
                        move(targetPos);
                    }
                }

                RECT frame = {0, 0, 0, 0};
                AdjustWindowRectEx(&frame, WS_OVERLAPPEDWINDOW, FALSE, 0);
                frame = {frame.left, frame.top, frame.right, frame.bottom};

                // record frame area data
                const double dpr = this->devicePixelRatioF();

                m_frames.setLeft(abs(frame.left) / dpr + 0.5);
                m_frames.setTop(abs(frame.bottom) / dpr + 0.5); // PS: 没有写错, frame.top 值有些异常, 原因不明
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
        case WM_MOVING:
            m_windowMoving = true;
            break;
        case WM_EXITSIZEMOVE :
            m_windowMoving = false;
            break;
        default:
            break;
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

void FramelessWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    if (m_bJustMaximized) {
        HWND hwnd = reinterpret_cast<HWND>(this->winId());
        HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO monitorInfo{};
        monitorInfo.cbSize = sizeof(MONITORINFO);
        GetMonitorInfo(monitor, &monitorInfo);
        const auto workRect = monitorInfo.rcWork;

        // 补偿阴影尺寸计算带来的边缘超出边界问题
        if (this->size().width() > workRect.right - workRect.left) {
            QMainWindow::setContentsMargins(m_frames + m_margins);
        }
    } else if (m_justNormaled) {
        m_justNormaled = false;
        m_frames = QMargins();
        QMainWindow::setContentsMargins(m_margins);
    }
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
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        case QEvent::ScreenChangeInternal: {
            // 通过设置Mask强制触发更新, 修正双屏拖拽时的错位问题, 同时会导致失去窗口阴影
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

void FramelessWindow::setContentsMargins(const QMargins& margins)
{
    QMainWindow::setContentsMargins(margins + m_frames);
    m_margins = margins;
}

void FramelessWindow::setContentsMargins(int left, int top, int right, int bottom)
{
    QMainWindow::setContentsMargins(left + m_frames.left(), top + m_frames.top(), right + m_frames.right(), bottom + m_frames.bottom());
    m_margins.setLeft(left);
    m_margins.setTop(top);
    m_margins.setRight(right);
    m_margins.setBottom(bottom);
}

QMargins FramelessWindow::contentsMargins() const
{
    QMargins margins = QMainWindow::contentsMargins();
    margins -= m_frames;
    return margins;
}

QRect FramelessWindow::contentsRect() const
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
