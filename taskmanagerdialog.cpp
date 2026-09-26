#include "taskmanagerdialog.h"
#include "webview.h"

#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEnginePage>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif

namespace {
// 返回某进程的常驻内存（字节），失败返回 -1
qint64 processRss(quint64 pid)
{
#ifdef Q_OS_WIN
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, DWORD(pid));
    if (!h)
        return -1;
    PROCESS_MEMORY_COUNTERS pmc;
    qint64 rss = -1;
    if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
        rss = qint64(pmc.WorkingSetSize);
    CloseHandle(h);
    return rss;
#else
    Q_UNUSED(pid);
    return -1;
#endif
}

QString formatBytes(qint64 b)
{
    if (b < 0)
        return QStringLiteral("-");
    const double mb = double(b) / (1024.0 * 1024.0);
    if (mb >= 1024)
        return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
    return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}
} // namespace

TaskManagerDialog::TaskManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("任务管理器 - Breeze"));
    resize(640, 380);

    auto *layout = new QVBoxLayout(this);

    auto *hint = new QLabel(QStringLiteral("显示各标签页的渲染进程与内存占用（每秒刷新）"), this);
    hint->setStyleSheet(QStringLiteral("color:#888;"));
    layout->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("标题"),
        QStringLiteral("URL"),
        QStringLiteral("PID"),
        QStringLiteral("内存")
    });
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->verticalHeader()->setVisible(false);
    layout->addWidget(m_table);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &TaskManagerDialog::refresh);
    m_timer->start();
}

void TaskManagerDialog::setTabs(const QList<WebView *> &tabs)
{
    m_tabs = tabs;
    refresh();
}

void TaskManagerDialog::refresh()
{
    // 去掉已销毁的标签
    QList<WebView *> alive;
    for (WebView *v : m_tabs) {
        if (v && v->page())
            alive.append(v);
    }
    m_tabs = alive;

    m_table->setRowCount(m_tabs.size());
    qint64 total = 0;
    for (int i = 0; i < m_tabs.size(); ++i) {
        WebView *v = m_tabs.at(i);
        auto *titleItem = new QTableWidgetItem(v->title().isEmpty()
            ? QStringLiteral("新标签页") : v->title());
        titleItem->setToolTip(v->title());
        m_table->setItem(i, 0, titleItem);
        m_table->setItem(i, 1, new QTableWidgetItem(v->url().toString()));

        const quint64 pid = v->page()->renderProcessPid();
        m_table->setItem(i, 2, new QTableWidgetItem(pid > 0
            ? QString::number(pid) : QStringLiteral("-")));

        const qint64 rss = pid > 0 ? processRss(pid) : -1;
        if (rss > 0)
            total += rss;
        m_table->setItem(i, 3, new QTableWidgetItem(formatBytes(rss)));
    }

    setWindowTitle(QStringLiteral("任务管理器（共 %1 个标签，合计 %2） - Breeze")
                       .arg(m_tabs.size()).arg(formatBytes(total)));
}
