#include "toolbox.h"
#include "qrcodegen.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QPainter>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

ToolboxDialog::ToolboxDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("工具箱 - Breeze"));
    resize(720, 560);

    auto *layout = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(createJsonTab(),     QStringLiteral("💻 JSON"));
    tabs->addTab(createEncodeTab(),   QStringLiteral("🔗 编解码"));
    tabs->addTab(createTimeTab(),     QStringLiteral("⏰ 时间"));
    tabs->addTab(createPasswordTab(), QStringLiteral("🎲 密码"));
    tabs->addTab(createColorTab(),    QStringLiteral("🌈 颜色"));
    tabs->addTab(createUnitTab(),     QStringLiteral("📏 单位"));
    tabs->addTab(createQrTab(),       QStringLiteral("📱 二维码"));
    tabs->addTab(createHashTab(),     QStringLiteral("🔐 哈希"));
    tabs->addTab(createTextStatTab(), QStringLiteral("📊 文本统计"));
    layout->addWidget(tabs);

    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    auto *bottom = new QHBoxLayout;
    bottom->addStretch();
    bottom->addWidget(closeBtn);
    layout->addLayout(bottom);
}

QWidget *ToolboxDialog::createJsonTab()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);

    auto *input = new QTextEdit(w);
    input->setPlaceholderText(QStringLiteral("粘贴 JSON…"));
    lay->addWidget(input);

    auto *btnRow = new QHBoxLayout;
    auto *formatBtn = new QPushButton(QStringLiteral("格式化"), w);
    auto *minifyBtn = new QPushButton(QStringLiteral("压缩"), w);
    btnRow->addWidget(formatBtn);
    btnRow->addWidget(minifyBtn);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    auto *output = new QTextEdit(w);
    output->setReadOnly(true);
    lay->addWidget(output);

    connect(formatBtn, &QPushButton::clicked, w, [input, output]() {
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(
            input->toPlainText().toUtf8(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            output->setPlainText(QStringLiteral("JSON 错误：") + parseErr.errorString());
            return;
        }
        output->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
    });
    connect(minifyBtn, &QPushButton::clicked, w, [input, output]() {
        QJsonParseError parseErr;
        const QJsonDocument doc = QJsonDocument::fromJson(
            input->toPlainText().toUtf8(), &parseErr);
        if (parseErr.error != QJsonParseError::NoError) {
            output->setPlainText(QStringLiteral("JSON 错误：") + parseErr.errorString());
            return;
        }
        output->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)));
    });
    return w;
}

QWidget *ToolboxDialog::createEncodeTab()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);

    auto *input = new QTextEdit(w);
    input->setPlaceholderText(QStringLiteral("输入文本…"));
    lay->addWidget(input);

    auto *btnRow = new QHBoxLayout;
    auto *urlEnc = new QPushButton(QStringLiteral("URL 编码"), w);
    auto *urlDec = new QPushButton(QStringLiteral("URL 解码"), w);
    auto *b64Enc = new QPushButton(QStringLiteral("Base64 编码"), w);
    auto *b64Dec = new QPushButton(QStringLiteral("Base64 解码"), w);
    for (auto *b : {urlEnc, urlDec, b64Enc, b64Dec})
        btnRow->addWidget(b);
    btnRow->addStretch();
    lay->addLayout(btnRow);

    auto *output = new QTextEdit(w);
    output->setReadOnly(true);
    lay->addWidget(output);

    connect(urlEnc, &QPushButton::clicked, w, [input, output]() {
        output->setPlainText(QString::fromUtf8(
            QUrl::toPercentEncoding(input->toPlainText())));
    });
    connect(urlDec, &QPushButton::clicked, w, [input, output]() {
        output->setPlainText(QUrl::fromPercentEncoding(
            input->toPlainText().toUtf8()));
    });
    connect(b64Enc, &QPushButton::clicked, w, [input, output]() {
        output->setPlainText(QString::fromUtf8(
            input->toPlainText().toUtf8().toBase64()));
    });
    connect(b64Dec, &QPushButton::clicked, w, [input, output]() {
        output->setPlainText(QString::fromUtf8(
            QByteArray::fromBase64(input->toPlainText().toUtf8())));
    });
    return w;
}

QWidget *ToolboxDialog::createTimeTab()
{
    auto *w = new QWidget;
    auto *lay = new QFormLayout(w);

    auto *tsEdit = new QLineEdit(w);
    tsEdit->setPlaceholderText(QStringLiteral("Unix 时间戳（秒）"));
    lay->addRow(QStringLiteral("时间戳："), tsEdit);

    auto *dateEdit = new QLineEdit(w);
    dateEdit->setPlaceholderText(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    lay->addRow(QStringLiteral("日期："), dateEdit);

    auto *btnRow = new QHBoxLayout;
    auto *toDate = new QPushButton(QStringLiteral("时间戳 → 日期"), w);
    auto *toTs = new QPushButton(QStringLiteral("日期 → 时间戳"), w);
    auto *nowBtn = new QPushButton(QStringLiteral("当前时间"), w);
    btnRow->addWidget(toDate);
    btnRow->addWidget(toTs);
    btnRow->addWidget(nowBtn);
    lay->addRow(btnRow);

    connect(toDate, &QPushButton::clicked, w, [tsEdit, dateEdit]() {
        const qint64 ts = tsEdit->text().trimmed().toLongLong();
        dateEdit->setText(QDateTime::fromSecsSinceEpoch(ts).toString(
            QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    });
    connect(toTs, &QPushButton::clicked, w, [tsEdit, dateEdit]() {
        const QDateTime dt = QDateTime::fromString(
            dateEdit->text().trimmed(), QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (dt.isValid())
            tsEdit->setText(QString::number(dt.toSecsSinceEpoch()));
    });
    connect(nowBtn, &QPushButton::clicked, w, [tsEdit, dateEdit]() {
        const QDateTime now = QDateTime::currentDateTime();
        tsEdit->setText(QString::number(now.toSecsSinceEpoch()));
        dateEdit->setText(now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    });
    return w;
}

QWidget *ToolboxDialog::createPasswordTab()
{
    auto *w = new QWidget;
    auto *lay = new QFormLayout(w);

    auto *lenSpin = new QSpinBox(w);
    lenSpin->setRange(4, 128);
    lenSpin->setValue(16);
    lay->addRow(QStringLiteral("长度："), lenSpin);

    auto *upper = new QCheckBox(QStringLiteral("大写字母"), w);
    upper->setChecked(true);
    auto *lower = new QCheckBox(QStringLiteral("小写字母"), w);
    lower->setChecked(true);
    auto *digits = new QCheckBox(QStringLiteral("数字"), w);
    digits->setChecked(true);
    auto *symbols = new QCheckBox(QStringLiteral("符号"), w);
    symbols->setChecked(true);
    lay->addRow(upper);
    lay->addRow(lower);
    lay->addRow(digits);
    lay->addRow(symbols);

    auto *result = new QLineEdit(w);
    result->setReadOnly(true);
    lay->addRow(QStringLiteral("结果："), result);

    auto gen = [=]() {
        QString chars;
        if (upper->isChecked())   chars += QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZ");
        if (lower->isChecked())   chars += QStringLiteral("abcdefghijklmnopqrstuvwxyz");
        if (digits->isChecked())  chars += QStringLiteral("0123456789");
        if (symbols->isChecked()) chars += QStringLiteral("!@#$%^&*()-_=+[]{};:,.<>?");
        if (chars.isEmpty()) {
            result->setText(QStringLiteral("（请至少选择一类字符）"));
            return;
        }
        QString pwd;
        for (int i = 0; i < lenSpin->value(); ++i)
            pwd += chars.at(QRandomGenerator::global()->bounded(chars.size()));
        result->setText(pwd);
    };

    auto *btnRow = new QHBoxLayout;
    auto *genBtn = new QPushButton(QStringLiteral("生成"), w);
    auto *copyBtn = new QPushButton(QStringLiteral("复制"), w);
    connect(genBtn, &QPushButton::clicked, w, gen);
    connect(copyBtn, &QPushButton::clicked, w, [result]() {
        QApplication::clipboard()->setText(result->text());
    });
    btnRow->addWidget(genBtn);
    btnRow->addWidget(copyBtn);
    lay->addRow(btnRow);
    gen();
    return w;
}

QWidget *ToolboxDialog::createColorTab()
{
    auto *w = new QWidget;
    auto *lay = new QFormLayout(w);

    auto *hexEdit = new QLineEdit(w);
    hexEdit->setPlaceholderText(QStringLiteral("#RRGGBB"));
    lay->addRow(QStringLiteral("HEX："), hexEdit);

    auto *rgbEdit = new QLineEdit(w);
    rgbEdit->setPlaceholderText(QStringLiteral("255,255,255"));
    lay->addRow(QStringLiteral("RGB："), rgbEdit);

    auto *preview = new QLabel(w);
    preview->setMinimumHeight(60);
    lay->addRow(QStringLiteral("预览："), preview);

    auto update = [preview](const QColor &c) {
        preview->setStyleSheet(QStringLiteral("background:%1;border:1px solid #888;")
                                   .arg(c.name()));
    };

    connect(hexEdit, &QLineEdit::textChanged, w, [rgbEdit, update](const QString &t) {
        QColor c(t.trimmed());
        if (c.isValid()) {
            rgbEdit->setText(QStringLiteral("%1,%2,%3")
                                 .arg(c.red()).arg(c.green()).arg(c.blue()));
            update(c);
        }
    });
    connect(rgbEdit, &QLineEdit::textChanged, w, [hexEdit, update](const QString &t) {
        const QStringList parts = t.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() == 3) {
            QColor c(parts[0].trimmed().toInt(),
                     parts[1].trimmed().toInt(),
                     parts[2].trimmed().toInt());
            if (c.isValid()) {
                hexEdit->setText(c.name());
                update(c);
            }
        }
    });

    auto *pickBtn = new QPushButton(QStringLiteral("打开取色器"), w);
    connect(pickBtn, &QPushButton::clicked, w, [hexEdit, update, this]() {
        const QColor c = QColorDialog::getColor(Qt::white, this);
        if (c.isValid()) {
            hexEdit->setText(c.name());
            update(c);
        }
    });
    lay->addRow(pickBtn);
    return w;
}

QWidget *ToolboxDialog::createUnitTab()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);

    auto *typeCombo = new QComboBox(w);
    typeCombo->addItems({QStringLiteral("长度"), QStringLiteral("重量"), QStringLiteral("温度")});
    lay->addWidget(typeCombo);

    auto *fromCombo = new QComboBox(w);
    auto *toCombo = new QComboBox(w);
    auto *valueEdit = new QLineEdit(w);
    valueEdit->setPlaceholderText(QStringLiteral("数值"));
    auto *resultEdit = new QLineEdit(w);
    resultEdit->setReadOnly(true);

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("从："), fromCombo);
    form->addRow(QStringLiteral("到："), toCombo);
    form->addRow(QStringLiteral("数值："), valueEdit);
    form->addRow(QStringLiteral("结果："), resultEdit);
    lay->addLayout(form);

    auto fillUnits = [typeCombo, fromCombo, toCombo]() {
        fromCombo->clear();
        toCombo->clear();
        const QString type = typeCombo->currentText();
        if (type == QStringLiteral("长度"))
            fromCombo->addItems({"米", "千米", "厘米", "毫米", "英里", "英尺", "英寸"});
        else if (type == QStringLiteral("重量"))
            fromCombo->addItems({"千克", "克", "毫克", "吨", "磅", "盎司"});
        else
            fromCombo->addItems({"摄氏度", "华氏度", "开尔文"});
        toCombo->addItems(QStringList()
            << fromCombo->itemText(0) << fromCombo->itemText(1));
        if (fromCombo->count() > 1)
            toCombo->setCurrentIndex(1);
    };
    connect(typeCombo, &QComboBox::currentTextChanged, w, fillUnits);
    fillUnits();

    auto *convertBtn = new QPushButton(QStringLiteral("转换"), w);
    connect(convertBtn, &QPushButton::clicked, w,
            [fromCombo, toCombo, valueEdit, resultEdit]() {
        const double v = valueEdit->text().trimmed().toDouble();
        const QString f = fromCombo->currentText();
        const QString t = toCombo->currentText();

        const QMap<QString, double> lenToM = {
            {"米", 1}, {"千米", 1000}, {"厘米", 0.01}, {"毫米", 0.001},
            {"英里", 1609.34}, {"英尺", 0.3048}, {"英寸", 0.0254}};
        const QMap<QString, double> wToKg = {
            {"千克", 1}, {"克", 0.001}, {"毫克", 1e-6}, {"吨", 1000},
            {"磅", 0.4536}, {"盎司", 0.02835}};

        double out = 0;
        if (lenToM.contains(f) && lenToM.contains(t))
            out = v * lenToM[f] / lenToM[t];
        else if (wToKg.contains(f) && wToKg.contains(t))
            out = v * wToKg[f] / wToKg[t];
        else {
            // 温度
            double c = v;
            if (f == QStringLiteral("华氏度")) c = (v - 32) * 5 / 9;
            else if (f == QStringLiteral("开尔文")) c = v - 273.15;
            if (t == QStringLiteral("摄氏度")) out = c;
            else if (t == QStringLiteral("华氏度")) out = c * 9 / 5 + 32;
            else out = c + 273.15;
        }
        resultEdit->setText(QString::number(out, 'g', 8));
    });
    lay->addWidget(convertBtn);
    lay->addStretch();
    return w;
}

QWidget *ToolboxDialog::createQrTab()
{
    auto *w = new QWidget;
    auto *lay = new QVBoxLayout(w);

    auto *edit = new QLineEdit(w);
    edit->setPlaceholderText(QStringLiteral("输入文本或网址，回车生成二维码"));
    lay->addWidget(edit);

    auto *imageLabel = new QLabel(w);
    imageLabel->setAlignment(Qt::AlignCenter);
    imageLabel->setMinimumHeight(280);
    lay->addWidget(imageLabel);

    auto *saveBtn = new QPushButton(QStringLiteral("保存为 PNG"), w);
    lay->addWidget(saveBtn);

    auto generate = [edit, imageLabel]() {
        const QString text = edit->text().trimmed();
        if (text.isEmpty())
            return;
        const QPixmap pix = ToolboxDialog::makeQrPixmap(text);
        imageLabel->setPixmap(pix);
    };
    connect(edit, &QLineEdit::returnPressed, w, generate);
    connect(saveBtn, &QPushButton::clicked, w, [edit, imageLabel, this]() {
        const QPixmap pix = imageLabel->pixmap();
        if (pix.isNull())
            return;
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("保存二维码"), QStringLiteral("qrcode.png"),
            QStringLiteral("PNG 图片 (*.png)"));
        if (!path.isEmpty())
            pix.save(path);
    });
    return w;
}

// 二维码绘制：调用 qrcodegen 生成模块矩阵，用 QPainter 画
QPixmap ToolboxDialog::makeQrPixmap(const QString &text, int size)
{
    using namespace qrcodegen;
    // 纠错等级 M
    const QrCode qr = QrCode::encodeText(text.toUtf8().constData(), QrCode::Ecc::MEDIUM);
    const int n = qr.getSize();
    const int scale = qMax(2, size / (n + 4));   // 每模块像素
    const int margin = scale * 2;                // 静默区
    const int total = n * scale + margin * 2;

    QPixmap pix(total, total);
    pix.fill(Qt::white);
    QPainter p(&pix);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            if (qr.getModule(x, y))
                p.drawRect(margin + x * scale, margin + y * scale, scale, scale);
        }
    }
    p.end();
    return pix;
}

QWidget *ToolboxDialog::createHashTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *input = new QTextEdit(w);
    input->setPlaceholderText(QStringLiteral("输入要计算哈希的文本…"));
    input->setMaximumHeight(120);
    v->addWidget(input);

    auto *row = new QHBoxLayout;
    auto *algoCombo = new QComboBox(w);
    algoCombo->addItem(QStringLiteral("MD5"),    QVariant::fromValue(int(QCryptographicHash::Md5)));
    algoCombo->addItem(QStringLiteral("SHA-1"),  QVariant::fromValue(int(QCryptographicHash::Sha1)));
    algoCombo->addItem(QStringLiteral("SHA-256"),QVariant::fromValue(int(QCryptographicHash::Sha256)));
    algoCombo->addItem(QStringLiteral("SHA-512"),QVariant::fromValue(int(QCryptographicHash::Sha512)));
    auto *calcBtn = new QPushButton(QStringLiteral("计算"), w);
    auto *upperBtn = new QPushButton(QStringLiteral("大写"), w);
    row->addWidget(new QLabel(QStringLiteral("算法："), w));
    row->addWidget(algoCombo);
    row->addWidget(calcBtn);
    row->addWidget(upperBtn);
    row->addStretch();
    v->addLayout(row);

    auto *output = new QLineEdit(w);
    output->setReadOnly(true);
    output->setPlaceholderText(QStringLiteral("结果…"));
    v->addWidget(output);

    auto calc = [input, algoCombo, output]() {
        const auto algo = QCryptographicHash::Algorithm(
            algoCombo->currentData().toInt());
        const QByteArray hash = QCryptographicHash::hash(
            input->toPlainText().toUtf8(), algo);
        output->setText(QString::fromLatin1(hash.toHex()));
    };
    QObject::connect(calcBtn, &QPushButton::clicked, w, calc);
    QObject::connect(upperBtn, &QPushButton::clicked, w, [output]() {
        output->setText(output->text().toUpper());
    });

    v->addStretch();
    return w;
}

QWidget *ToolboxDialog::createTextStatTab()
{
    auto *w = new QWidget;
    auto *v = new QVBoxLayout(w);

    auto *input = new QTextEdit(w);
    input->setPlaceholderText(QStringLiteral("粘贴文本，实时统计…"));
    v->addWidget(input, 1);

    auto *stat = new QLabel(w);
    stat->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(stat);

    auto update = [input, stat]() {
        const QString text = input->toPlainText();
        const int chars     = text.size();
        const int noSpaces  = text.count(QRegularExpression(QStringLiteral("\\S")));
        const int lines     = text.isEmpty() ? 0 : text.count(QLatin1Char('\n')) + 1;
        const int words     = text.split(QRegularExpression(QStringLiteral("\\s+")),
                                         Qt::SkipEmptyParts).size();
        // 中文按字符计，英文按空格分词
        const int cjk = text.count(QRegularExpression(QStringLiteral("[\\x{4e00}-\\x{9fff}]")));
        stat->setText(QStringLiteral("字符：%1  ｜  不含空格：%2  ｜  行：%3  ｜  词：%4  ｜  汉字：%5")
                          .arg(chars).arg(noSpaces).arg(lines).arg(words).arg(cjk));
    };
    QObject::connect(input, &QTextEdit::textChanged, w, update);
    update();

    return w;
}
