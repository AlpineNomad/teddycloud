#include <assert.h>
#include <stdint.h>
#include <stdio.h>

uint64_t read_big_endian64(const uint8_t *buf);
uint64_t read_little_endian64(const uint8_t *buf);

int main(void)
{
    // RTNL sends the low big-endian word first. Bit 31 must not corrupt
    // the high word; preserve the existing word order for both helpers.
    const uint8_t big[][8] = {
        {0x92, 0x34, 0x56, 0x78, 0xe0, 0x04, 0x03, 0x00},
        {0x7f, 0xff, 0xff, 0xff, 0xe0, 0x04, 0x03, 0x00},
        {0x80, 0x00, 0x00, 0x00, 0xe0, 0x04, 0x03, 0x00},
        {0xff, 0xff, 0xff, 0xff, 0xe0, 0x04, 0x03, 0x00},
        {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01},
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        {0, 0, 0, 0, 0, 0, 0, 0}
    };
    const uint64_t expected[] = {
        UINT64_C(0xe004030092345678), UINT64_C(0xe00403007fffffff),
        UINT64_C(0xe004030080000000), UINT64_C(0xe0040300ffffffff),
        UINT64_C(0x0000000180000000), UINT64_MAX, 0
    };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        uint8_t little[8];
        for (size_t j = 0; j < 8; j++) little[j] = big[i][7 - j];
        assert(read_big_endian64(big[i]) == expected[i]);
        assert(read_little_endian64(little) == expected[i]);
    }
    puts("PASS: real UID readers preserve both words, including bit-31 boundaries");
    return 0;
}
