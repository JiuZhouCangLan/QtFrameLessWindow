#ifndef CFRAMELESSWINDOW_H
#define CFRAMELESSWINDOW_H
#include <QObject>
#include <QMainWindow>
#include <QWidget>
#include <QList>
#include <QMargins>
#include <QRect>

class FramelessWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = 0);

    // if resizeable is set to false, then the window can not be resized by mouse
    // but still can be resized programtically
    void setResizeable(bool resizeable = true);
    bool isResizeable() const;

    // set border width, inside this aera, window can be resized by mouse
    void setResizeableAreaWidth(int width = 5);

    // 继承而来的非虚函数
    void setContentsMargins(const QMargins &margins);
    void setContentsMargins(int left, int top, int right, int bottom);
    QMargins contentsMargins() const;
    QRect contentsRect() const;

protected:
    //set a widget which will be treat as SYSTEM titlebar
    void setTitleBar(QWidget* titlebar);

    //generally, we can add widget say "label1" on titlebar, and it will cover the titlebar under it
    //as a result, we can not drag and move the MainWindow with this "label1" again
    //we can fix this by add "label1" to a ignorelist, just call addIgnoreWidget(label1)
    void addIgnoreWidget(QWidget* widget);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;
#else
    bool nativeEvent(const QByteArray &eventType, void *message, long *result) override;
#endif
    bool event(QEvent *event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QWidget* m_titlebar;
    QList<QWidget*> m_whiteList;
    int m_borderWidth;
    QMargins m_margins;
    QMargins m_frames;
    bool m_bJustMaximized;
    bool m_bResizeable;
    bool m_justNormaled = false;
    bool m_windowMoving = false; // 窗口正在移动标识

private slots:
    void onTitleBarDestroyed();
};

#endif // CFRAMELESSWINDOW_H
