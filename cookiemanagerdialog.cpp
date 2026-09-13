#include "cookiemanagerdialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWebEngineCookieStore>
#include <QWebEngineProfile>

CookieManagerDialog::CookieManagerDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Cookie 管理 - Breeze"));
    resize(720, 460);

    auto *layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({QStringLiteral("域"), QStringLiteral("名称"), QStringLiteral("值")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *btnRow = new QHBoxLayout;
    auto *reloadBtn = new QPushButton(QStringLiteral("刷新"), this);
    auto *delSelBtn = new QPushButton(QStringLiteral("删除选中"), this);
    auto *delAllBtn = new QPushButton(QStringLiteral("删除全部"), this);
    auto *closeBtn  = new QPushButton(QStringLiteral("关闭"), this);
    btnRow->addWidget(reloadBtn);
    btnRow->addStretch();
    btnRow->addWidget(delSelBtn);
    btnRow->addWidget(delAllBtn);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    connect(reloadBtn, &QPushButton::clicked, this, &CookieManagerDialog::reload);
    connect(delSelBtn, &QPushButton::clicked, this, &CookieManagerDialog::onDeleteSelected);
    connect(delAllBtn, &QPushButton::clicked, this, &CookieManagerDialog::onDeleteAll);
    connect(closeBtn,  &QPushButton::clicked, this, &QDialog::accept);

    m_store = QWebEngineProfile::defaultProfile()->cookieStore();

    reload();
}

void CookieManagerDialog::reload()
{
    m_cookies.clear();
    m_table->setRowCount(0);

    if (!m_store)
        return;
    if (m_loading)
        return;
    m_loading = true;

    // 单次连接：loadAllCookies 会逐个发 cookieAdded，最后发 loadFinished
    connect(m_store, &QWebEngineCookieStore::cookieAdded, this,
            [this](const QNetworkCookie &c) {
                if (!m_loading)
                    return;
                m_cookies.append(c);
                const int row = m_table->rowCount();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(c.domain()));
                m_table->setItem(row, 1, new QTableWidgetItem(QString::fromUtf8(c.name())));
                QString v = QString::fromUtf8(c.value());
                if (v.length() > 80) v = v.left(80) + QStringLiteral("…");
                m_table->setItem(row, 2, new QTableWidgetItem(v));
            },
            Qt::UniqueConnection);

    m_store->loadAllCookies();
    // loadAllCookies 无明确的"完成"信号，延时关闭 loading 标志
    QTimer::singleShot(1500, this, [this]() { m_loading = false; });
}

int CookieManagerDialog::currentRow() const
{
    const auto rows = m_table->selectionModel()
        ? m_table->selectionModel()->selectedRows() : QModelIndexList();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void CookieManagerDialog::onDeleteSelected()
{
    const int row = currentRow();
    if (row < 0 || row >= m_cookies.size())
        return;
    m_store->deleteCookie(m_cookies.at(row));
    m_cookies.removeAt(row);
    m_table->removeRow(row);
}

void CookieManagerDialog::onDeleteAll()
{
    if (QMessageBox::question(this, QStringLiteral("删除全部 Cookie"),
            QStringLiteral("确定删除所有 Cookie？此操作不可撤销。"))
        != QMessageBox::Yes)
        return;
    m_store->deleteAllCookies();
    m_cookies.clear();
    m_table->setRowCount(0);
}
