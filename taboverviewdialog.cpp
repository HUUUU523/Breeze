#include "taboverviewdialog.h"

#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWebEngineView>

// 单个标签卡片：缩略图 + 标题 + 关闭按钮
static QWidget *makeCard(QTabWidget *tabs, int index, QWidget *parent)
{
    auto *card = new QWidget(parent);
    card->setFixedSize(240, 180);
    card->setStyleSheet(QStringLiteral(
        "QWidget#card{background:#2b2b2b;border-radius:8px;}"
        "QWidget#card:hover{background:#3a3a3a;}"));

    auto *lay = new QVBoxLayout(card);
    lay->setContentsMargins(6, 6, 6, 6);
    lay->setSpacing(4);

    // 缩略图
    auto *thumb = new QLabel(card);
    thumb->setFixedSize(228, 130);
    thumb->setAlignment(Qt::AlignCenter);
    thumb->setStyleSheet(QStringLiteral("background:#1e1e1e;border-radius:4px;"));
    if (auto *view = qobject_cast<QWebEngineView *>(tabs->widget(index))) {
        const QPixmap shot = view->grab();
        if (!shot.isNull())
            thumb->setPixmap(shot.scaled(thumb->size(), Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
        else
            thumb->setText(QStringLiteral("(无预览)"));
    } else {
        thumb->setText(QStringLiteral("(空标签)"));
    }
    lay->addWidget(thumb);

    // 标题行 + 关闭按钮
    auto *row = new QWidget(card);
    auto *rowLay = new QHBoxLayout(row);
    rowLay->setContentsMargins(0, 0, 0, 0);
    rowLay->setSpacing(4);

    auto *title = new QLabel(tabs->tabText(index), row);
    title->setStyleSheet(QStringLiteral("color:#ddd;font-size:12px;"));
    title->setWordWrap(false);
    title->setToolTip(tabs->tabText(index));
    rowLay->addWidget(title, 1);

    auto *closeBtn = new QToolButton(row);
    closeBtn->setText(QStringLiteral("×"));
    closeBtn->setFixedSize(20, 20);
    closeBtn->setStyleSheet(QStringLiteral(
        "QToolButton{color:#aaa;border:none;font-size:16px;}"
        "QToolButton:hover{color:#fff;background:#c0392b;border-radius:3px;}"));
    rowLay->addWidget(closeBtn);

    lay->addWidget(row);

    return card;
}

TabOverviewDialog::TabOverviewDialog(QTabWidget *tabs, QWidget *parent)
    : QDialog(parent), m_tabs(tabs)
{
    setWindowTitle(QStringLiteral("标签总览"));
    resize(820, 600);
    setStyleSheet(QStringLiteral("background:#1a1a1a;"));

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet(QStringLiteral("border:none;background:transparent;"));
    auto *holder = new QWidget(scroll);
    auto *grid = new QGridLayout(holder);
    grid->setSpacing(12);
    grid->setContentsMargins(0, 0, 0, 0);

    const int count = m_tabs->count();
    const int cols = 3;
    for (int i = 0; i < count; ++i) {
        auto *card = makeCard(m_tabs, i, holder);
        // 用事件过滤器实现点击卡片
        card->installEventFilter(this);
        card->setProperty("breezeTabIndex", i);
        grid->addWidget(card, i / cols, i % cols);

        if (auto *closeBtn = card->findChild<QToolButton *>()) {
            connect(closeBtn, &QToolButton::clicked, this, [this, i]() {
                emit tabClosed(i);
                accept();
            });
        }
    }
    grid->setRowStretch(grid->rowCount(), 1);
    scroll->setWidget(holder);
    outer->addWidget(scroll);

    auto *hint = new QLabel(QStringLiteral("点击卡片切换标签，× 关闭标签。"), this);
    hint->setStyleSheet(QStringLiteral("color:#888;font-size:12px;"));
    outer->addWidget(hint);
}

bool TabOverviewDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        const QVariant idx = obj->property("breezeTabIndex");
        if (idx.isValid()) {
            emit tabActivated(idx.toInt());
            accept();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}
