#include "syncdialog.h"
#include "browserwindow.h"
#include "syncmanager.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

SyncDialog::SyncDialog(BrowserWindow *browser)
    : QDialog(browser), m_browser(browser)
{
    setWindowTitle(QStringLiteral("云同步 - Breeze"));
    resize(520, 340);

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("https://example.com/remote.php/dav/files/user/"));
    m_userEdit = new QLineEdit(this);
    m_passEdit = new QLineEdit(this);
    m_passEdit->setEchoMode(QLineEdit::Password);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setText(QStringLiteral("breeze-sync.dat"));
    m_passphraseEdit = new QLineEdit(this);
    m_passphraseEdit->setEchoMode(QLineEdit::Password);
    m_passphraseEdit->setPlaceholderText(QStringLiteral("加密口令（所有设备须一致）"));

    form->addRow(QStringLiteral("服务器："), m_urlEdit);
    form->addRow(QStringLiteral("用户名："), m_userEdit);
    form->addRow(QStringLiteral("密码："), m_passEdit);
    form->addRow(QStringLiteral("云端文件："), m_pathEdit);
    form->addRow(QStringLiteral("加密口令："), m_passphraseEdit);
    layout->addLayout(form);

    layout->addWidget(new QLabel(
        QStringLiteral("上传 = 用本机数据覆盖云端；下载 = 用云端数据覆盖本机。"), this));

    auto *btns = new QDialogButtonBox(this);
    auto *upBtn = btns->addButton(QStringLiteral("上传"), QDialogButtonBox::ActionRole);
    auto *downBtn = btns->addButton(QStringLiteral("下载"), QDialogButtonBox::ActionRole);
    btns->addButton(QDialogButtonBox::Close);
    layout->addWidget(btns);

    connect(upBtn, &QPushButton::clicked, this, &SyncDialog::onUpload);
    connect(downBtn, &QPushButton::clicked, this, &SyncDialog::onDownload);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::accept);

    loadConfig();
}

void SyncDialog::loadConfig()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    m_urlEdit->setText(s.value(QStringLiteral("sync/url")).toString());
    m_userEdit->setText(s.value(QStringLiteral("sync/user")).toString());
    m_pathEdit->setText(s.value(QStringLiteral("sync/path"),
                                QStringLiteral("breeze-sync.dat")).toString());
}

void SyncDialog::saveConfig()
{
    QSettings s(QStringLiteral("Breeze"), QStringLiteral("Breeze"));
    s.setValue(QStringLiteral("sync/url"), m_urlEdit->text().trimmed());
    s.setValue(QStringLiteral("sync/user"), m_userEdit->text());
    s.setValue(QStringLiteral("sync/path"), m_pathEdit->text().trimmed());
}

void SyncDialog::onUpload()
{
    saveConfig();
    auto *mgr = new SyncManager(this);
    mgr->setServer(m_urlEdit->text().trimmed(), m_userEdit->text(), m_passEdit->text());
    mgr->setRemotePath(m_pathEdit->text().trimmed());
    mgr->setPassphrase(m_passphraseEdit->text());

    // 真正的双向同步：先下载云端，与本地合并，再上传合并结果
    const QByteArray local = m_browser->exportSyncData();
    connect(mgr, &SyncManager::downloadFinished, this,
            [this, mgr, local](bool ok, const QByteArray &remote, const QString &msg) {
                Q_UNUSED(msg);
                // 云端不存在（首次同步）→ 直接上传本地
                const QByteArray merged = ok && !remote.isEmpty()
                    ? BrowserWindow::mergeSyncData(local, remote)
                    : local;

                connect(mgr, &SyncManager::uploadFinished, this,
                        [this, mgr](bool upOk, const QString &upMsg) {
                            mgr->deleteLater();
                            QMessageBox::information(this, QStringLiteral("云同步"),
                                upOk ? QStringLiteral("同步成功（已合并）")
                                     : QStringLiteral("上传失败：") + upMsg);
                        });
                mgr->upload(merged);
            });
    mgr->download();
}

void SyncDialog::onDownload()
{
    saveConfig();
    auto *mgr = new SyncManager(this);
    mgr->setServer(m_urlEdit->text().trimmed(), m_userEdit->text(), m_passEdit->text());
    mgr->setRemotePath(m_pathEdit->text().trimmed());
    mgr->setPassphrase(m_passphraseEdit->text());

    connect(mgr, &SyncManager::downloadFinished, this,
            [this, mgr](bool ok, const QByteArray &data, const QString &msg) {
                mgr->deleteLater();
                if (!ok) {
                    QMessageBox::warning(this, QStringLiteral("云同步"),
                        QStringLiteral("下载失败：") + msg);
                    return;
                }
                if (QMessageBox::question(this, QStringLiteral("云同步"),
                        QStringLiteral("用云端数据覆盖本机书签/历史/设置？"))
                    != QMessageBox::Yes)
                    return;
                if (m_browser->importSyncData(data))
                    QMessageBox::information(this, QStringLiteral("云同步"),
                        QStringLiteral("已应用云端数据"));
                else
                    QMessageBox::warning(this, QStringLiteral("云同步"),
                        QStringLiteral("数据格式无效"));
            });
    mgr->download();
}
