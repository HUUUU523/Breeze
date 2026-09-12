// 精简版 QR Code 生成库（基于 Nayuki QR Code generator，MIT License 思路重写）
// 仅支持字节模式，纠错等级 L/M/Q/H，自动选择最小版本（1-40 中的常见版本）。
#ifndef QRCODEGEN_H
#define QRCODEGEN_H

#include <cstdint>
#include <string>
#include <vector>

namespace qrcodegen {

class QrCode
{
public:
    enum class Ecc { LOW, MEDIUM, QUARTILE, HIGH };

    // 用字节数据生成二维码
    static QrCode encodeText(const char *text, Ecc ecc);

    int getSize() const { return m_size; }
    bool getModule(int x, int y) const;

private:
    QrCode(int size, Ecc ecc);
    void initModules();
    void drawFunctionPatterns();
    void drawFinderPattern(int x, int y);
    void drawAlignmentPattern(int x, int y);
    void drawFormatBits(int mask);
    void drawCodewords(const std::vector<uint8_t> &data);
    void applyMask(int mask);
    void addEccAndInterleave(const std::vector<uint8_t> &data, std::vector<uint8_t> &result);

    int m_size;
    Ecc m_ecc;
    std::vector<std::vector<bool>> m_modules;
    std::vector<std::vector<bool>> m_isFunction;
};

} // namespace qrcodegen

#endif // QRCODEGEN_H
