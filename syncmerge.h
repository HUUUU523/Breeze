#ifndef SYNCMERGE_H
#define SYNCMERGE_H

#include <QByteArray>

// 同步数据合并（纯逻辑，无 UI 依赖）：
// 书签按 URL 去重取并集（本地优先），历史按 URL 去重取较新时间。
namespace SyncMerge {

QByteArray merge(const QByteArray &local, const QByteArray &remote);

} // namespace SyncMerge

#endif // SYNCMERGE_H
