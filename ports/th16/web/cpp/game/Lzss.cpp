#include "Lzss.hpp"

namespace th16 {

bool Lzss::decode(const u8* input, u32 size, u8* output, u32 capacity, u32& written) noexcept {
    u32 cursor = 0, head = 1, buffer = 0, available = 0;
    written = 0;
    auto bits = [&](u32 count) {
        while (available < count) {
            buffer = (buffer << 8) | (cursor < size ? input[cursor++] : 0);
            available += 8;
        }
        available -= count;
        return (buffer >> available) & ((1u << count) - 1);
    };
    auto put = [&](u8 value) {
        output[written++] = value;
        dictionary[head] = value;
        head = (head + 1) & 8191;
    };
    for (;;) {
        if (bits(1)) {
            if (written == capacity) return false;
            put(u8(bits(8)));
        } else {
            const u32 offset = bits(13);
            if (!offset) return true;
            const u32 length = bits(4) + 3;
            if (length > capacity - written) return false;
            for (u32 i = 0; i < length; ++i) put(dictionary[(offset + i) & 8191]);
        }
    }
}

}  // namespace th16
