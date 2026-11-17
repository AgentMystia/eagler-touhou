// th16 LZSS 解码——原版 0x457f00（档案条目与目录表）的行为镜像。
// 13 位字典偏移、4 位长度(+3)、8192 字节字典，字典游标从 1 起步；偏移 0 为终止符。
// 位读取越过输入末尾时以 0 填充（与原版缺省终止符的行为一致）。
#pragma once
#include "Types.hpp"

namespace th16 {

class Lzss {
public:
    u8 dictionary[8192]{};
    bool decode(const u8* input, u32 size, u8* output, u32 capacity, u32& written) noexcept;
};

}  // namespace th16
