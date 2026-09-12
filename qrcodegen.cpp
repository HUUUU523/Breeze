// 精简版 QR Code 生成（字节模式）。算法参考 QR Code Model 2 标准。
#include "qrcodegen.h"

#include <algorithm>
#include <cstring>

namespace qrcodegen {

namespace {

int gfMul(int x, int y) {
    int z = 0;
    for (int i = 7; i >= 0; --i) {
        z = (z << 1) ^ ((z >> 7) * 0x11D);
        z ^= ((y >> i) & 1) * x;
    }
    return z & 0xFF;
}

std::vector<uint8_t> rsComputeDivisor(int degree) {
    std::vector<uint8_t> result(degree, 0);
    result[degree - 1] = 1;
    int root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[j] = static_cast<uint8_t>(gfMul(result[j], root));
            if (j + 1 < degree)
                result[j] ^= result[j + 1];
        }
        root = gfMul(root, 0x02);
    }
    return result;
}

std::vector<uint8_t> rsComputeRemainder(const std::vector<uint8_t> &data,
                                        const std::vector<uint8_t> &divisor) {
    std::vector<uint8_t> result(divisor.size(), 0);
    for (uint8_t b : data) {
        const uint8_t factor = b ^ result[0];
        result.erase(result.begin());
        result.push_back(0);
        for (size_t i = 0; i < divisor.size(); ++i)
            result[i] ^= gfMul(divisor[i], factor);
    }
    return result;
}

struct EccInfo {
    int eccPerBlock;
    int totalDataCodewords;
};

// 版本 1-10，纠错等级 M 的每块纠错码字数与总数据码字数
EccInfo eccTable(int version) {
    static const int eccPerBlockM[] = {10, 16, 26, 18, 24, 16, 18, 22, 22, 26};
    static const int dataCodewordsM[] = {16, 28, 44, 64, 86, 108, 124, 154, 182, 216};
    const int v = std::max(1, std::min(10, version)) - 1;
    EccInfo info{};
    info.eccPerBlock = eccPerBlockM[v];
    info.totalDataCodewords = dataCodewordsM[v];
    return info;
}

} // namespace

QrCode::QrCode(int size, Ecc ecc)
    : m_size(size), m_ecc(ecc),
      m_modules(size, std::vector<bool>(size, false)),
      m_isFunction(size, std::vector<bool>(size, false))
{
}

bool QrCode::getModule(int x, int y) const {
    if (x < 0 || y < 0 || x >= m_size || y >= m_size)
        return false;
    return m_modules[y][x];
}

QrCode QrCode::encodeText(const char *text, Ecc ecc) {
    const std::vector<uint8_t> data(text, text + std::strlen(text));

    int version = 1;
    EccInfo info{};
    for (; version <= 10; ++version) {
        info = eccTable(version);
        const int capacityBits = info.totalDataCodewords * 8;
        const int neededBits = 4 + 8 + static_cast<int>(data.size()) * 8;
        if (neededBits <= capacityBits)
            break;
    }
    if (version > 10)
        version = 10;
    info = eccTable(version);

    const int size = version * 4 + 17;
    QrCode qr(size, ecc);

    const int capacityBits = info.totalDataCodewords * 8;

    std::vector<bool> bits;
    auto appendBits = [&bits](int val, int len) {
        for (int i = len - 1; i >= 0; --i)
            bits.push_back(((val >> i) & 1) != 0);
    };
    appendBits(0x4, 4);
    appendBits(static_cast<int>(data.size()), 8);
    for (uint8_t b : data)
        appendBits(b, 8);
    for (int i = 0; i < 4 && static_cast<int>(bits.size()) < capacityBits; ++i)
        bits.push_back(false);
    while (bits.size() % 8 != 0)
        bits.push_back(false);
    for (int pad = 0; static_cast<int>(bits.size()) < capacityBits; ++pad)
        appendBits((pad % 2 == 0) ? 0xEC : 0x11, 8);

    std::vector<uint8_t> dataCodewords(bits.size() / 8, 0);
    for (size_t i = 0; i < bits.size(); ++i)
        if (bits[i])
            dataCodewords[i >> 3] |= (0x80 >> (i & 7));

    std::vector<uint8_t> finalCodewords;
    qr.addEccAndInterleave(dataCodewords, finalCodewords);

    qr.drawFunctionPatterns();
    qr.drawCodewords(finalCodewords);
    qr.applyMask(0);
    qr.drawFormatBits(0);

    return qr;
}

void QrCode::drawFunctionPatterns() {
    // 定时图案
    for (int i = 0; i < m_size; ++i) {
        m_modules[6][i] = (i % 2 == 0); m_isFunction[6][i] = true;
        m_modules[i][6] = (i % 2 == 0); m_isFunction[i][6] = true;
    }
    // 三个定位图案
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            const bool on = (i == 0 || i == 6 || j == 0 || j == 6 ||
                             (i >= 2 && i <= 4 && j >= 2 && j <= 4));
            m_modules[i][j] = on; m_isFunction[i][j] = true;
            m_modules[i][m_size - 7 + j] = on; m_isFunction[i][m_size - 7 + j] = true;
            m_modules[m_size - 7 + i][j] = on; m_isFunction[m_size - 7 + i][j] = true;
        }
    }
    // 格式信息区域标记为功能模块
    for (int i = 0; i < 9; ++i) {
        if (i != 6) { m_isFunction[8][i] = true; m_isFunction[i][8] = true; }
    }
    for (int i = 0; i < 8; ++i) {
        m_isFunction[8][m_size - 1 - i] = true;
        m_isFunction[m_size - 1 - i][8] = true;
    }
    m_isFunction[m_size - 8][8] = true;

    // 对齐图案（版本 >= 2）
    const int version = (m_size - 17) / 4;
    if (version >= 2)
        drawAlignmentPattern(m_size - 7, m_size - 7);
}

void QrCode::drawAlignmentPattern(int x, int y) {
    for (int dy = -2; dy <= 2; ++dy)
        for (int dx = -2; dx <= 2; ++dx) {
            const int xx = x + dx, yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= m_size || yy >= m_size)
                continue;
            const bool on = (std::max(std::abs(dx), std::abs(dy)) != 1);
            m_modules[yy][xx] = on;
            m_isFunction[yy][xx] = true;
        }
}

void QrCode::drawFinderPattern(int, int) {}

void QrCode::addEccAndInterleave(const std::vector<uint8_t> &data,
                                 std::vector<uint8_t> &result) {
    const int version = (m_size - 17) / 4;
    const EccInfo info = eccTable(version);
    const std::vector<uint8_t> divisor = rsComputeDivisor(info.eccPerBlock);
    const std::vector<uint8_t> eccBytes = rsComputeRemainder(data, divisor);
    result = data;
    result.insert(result.end(), eccBytes.begin(), eccBytes.end());
}

void QrCode::drawCodewords(const std::vector<uint8_t> &data) {
    int i = 0;
    for (int right = m_size - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < m_size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? m_size - 1 - vert : vert;
                if (!m_isFunction[y][x] && i < static_cast<int>(data.size()) * 8) {
                    m_modules[y][x] = ((data[i >> 3] >> (7 - (i & 7))) & 1) != 0;
                    ++i;
                }
            }
        }
    }
}

void QrCode::applyMask(int mask) {
    for (int y = 0; y < m_size; ++y) {
        for (int x = 0; x < m_size; ++x) {
            if (m_isFunction[y][x])
                continue;
            bool invert = false;
            switch (mask) {
            case 0: invert = ((x + y) % 2 == 0); break;
            case 1: invert = (y % 2 == 0); break;
            case 2: invert = (x % 3 == 0); break;
            case 3: invert = ((x + y) % 3 == 0); break;
            case 4: invert = ((x / 3 + y / 2) % 2 == 0); break;
            default: invert = ((x + y) % 2 == 0); break;
            }
            if (invert)
                m_modules[y][x] = !m_modules[y][x];
        }
    }
}

void QrCode::drawFormatBits(int mask) {
    // 格式信息：纠错等级 M (00) + 掩码，BCH(15,5) + 固定掩码 0x5412
    const int eccBits = 0;   // M 级 = 00
    const int data = (eccBits << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bits = ((data << 10) | rem) ^ 0x5412;

    auto setModule = [this](int x, int y, bool on) {
        if (x >= 0 && y >= 0 && x < m_size && y < m_size)
            m_modules[y][x] = on;
    };

    // 左上
    for (int i = 0; i <= 5; ++i)
        setModule(8, i, (bits >> i) & 1);
    setModule(8, 7, (bits >> 6) & 1);
    setModule(8, 8, (bits >> 7) & 1);
    setModule(7, 8, (bits >> 8) & 1);
    for (int i = 9; i < 15; ++i)
        setModule(14 - i, 8, (bits >> i) & 1);

    // 右上 / 左下
    for (int i = 0; i < 8; ++i)
        setModule(m_size - 1 - i, 8, (bits >> i) & 1);
    for (int i = 8; i < 15; ++i)
        setModule(8, m_size - 15 + i, (bits >> i) & 1);
    setModule(8, m_size - 8, true);   // 固定暗模块
}

} // namespace qrcodegen
