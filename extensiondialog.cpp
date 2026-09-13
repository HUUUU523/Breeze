#include "extensiondialog.h"
#include "extension.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

ExtensionDialog::ExtensionDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("扩展管理 - Breeze"));
    resize(720, 420);

    auto *layout = new QVBoxLayout(this);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({QStringLiteral("名称"), QStringLiteral("版本"), QStringLiteral("目录")});
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table, 1);

    auto *row = new QHBoxLayout;
    auto *addBtn = new QPushButton(QStringLiteral("添加扩展目录…"), this);
    auto *delBtn = new QPushButton(QStringLiteral("移除"), this);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    row->addWidget(addBtn);
    row->addWidget(delBtn);
    row->addStretch();
    row->addWidget(closeBtn);
    layout->addLayout(row);

    connect(addBtn, &QPushButton::clicked, this, &ExtensionDialog::onAdd);
    connect(delBtn, &QPushButton::clicked, this, &ExtensionDialog::onRemove);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    reload();
}

void ExtensionDialog::reload()
{
    m_table->setRowCount(0);
    const QList<Extension> exts = ExtensionManager::loadAll();
    for (const Extension &e : exts) {
        const int row = m_table->rowCount();
        m_table->insertRow(row);
        m_table->setItem(row, 0, new QTableWidgetItem(e.name));
        m_table->setItem(row, 1, new QTableWidgetItem(e.version));
        m_table->setItem(row, 2, new QTableWidgetItem(e.dir));
    }
}

void ExtensionDialog::onAdd()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择扩展目录（需含 manifest.json）"));
    if (dir.isEmpty())
        return;

    if (!QFile::exists(dir + QStringLiteral("/manifest.json"))) {
        QMessageBox::warning(this, QStringLiteral("无效扩展"),
                             QStringLiteral("该目录下没有 manifest.json。"));
        return;
    }

    ExtensionManager::addDir(dir);
    reload();
}

void ExtensionDialog::onRemove()
{
    const int row = m_table->currentRow();
    if (row < 0)
        return;
    const QString dir = m_table->item(row, 2)->text();
    ExtensionManager::removeDir(dir);
    reload();
}
