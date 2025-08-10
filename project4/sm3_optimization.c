#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <immintrin.h>

// 循环左移宏
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

// 常量定义
static const uint32_t T[64] = {
    0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519,
    0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A
};

// 布尔函数
#define FF0(x, y, z) ((x) ^ (y) ^ (z))
#define GG0(x, y, z) ((x) ^ (y) ^ (z))
#define FF1(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define GG1(x, y, z) (((x) & (y)) | ((~(x)) & (z)))

// 置换函数
#define P0(x) ((x) ^ ROTL(x, 9) ^ ROTL(x, 17))
#define P1(x) ((x) ^ ROTL(x, 15) ^ ROTL(x, 23))

// 消息填充
void sm3_pad(const uint8_t *msg, size_t len, uint8_t **out, size_t *out_len) {
    size_t blocks = (len + 1 + 8 + 63) / 64;
    *out_len = blocks * 64;
    *out = (uint8_t *)malloc(*out_len);
    memset(*out, 0, *out_len);
    memcpy(*out, msg, len);
    (*out)[len] = 0x80; // 添加比特1

    // 添加长度（大端序）
    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) {
        (*out)[*out_len - 8 + i] = (bit_len >> (56 - i * 8)) & 0xFF;
    }
}

// 消息扩展
void sm3_expand(const uint8_t block[64], uint32_t W[68], uint32_t W1[64]) {
    for (int i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               block[i * 4 + 3];
    }
    for (int i = 16; i < 68; i++) {
        W[i] = P1(W[i - 16] ^ W[i - 9] ^ ROTL(W[i - 3], 15)) ^ 
               ROTL(W[i - 13], 7) ^ W[i - 6];
    }
    for (int i = 0; i < 64; i++) {
        W1[i] = W[i] ^ W[i + 4];
    }
}

// 压缩函数
void sm3_compress(uint32_t state[8], const uint32_t W[68], const uint32_t W1[64]) {
    uint32_t A = state[0], B = state[1], C = state[2], D = state[3];
    uint32_t E = state[4], F = state[5], G = state[6], H = state[7];

    for (int j = 0; j < 64; j++) {
        uint32_t SS1 = ROTL((ROTL(A, 12) + E + ROTL(T[j], j)), 7);
        uint32_t SS2 = SS1 ^ ROTL(A, 12);
        uint32_t TT1 = (j < 16 ? FF0(A, B, C) : FF1(A, B, C)) + D + SS2 + W1[j];
        uint32_t TT2 = (j < 16 ? GG0(E, F, G) : GG1(E, F, G)) + H + SS1 + W[j];
        D = C; C = ROTL(B, 9); B = A; A = TT1;
        H = G; G = ROTL(F, 19); F = E; E = P0(TT2);
    }

    state[0] ^= A; state[1] ^= B; state[2] ^= C; state[3] ^= D;
    state[4] ^= E; state[5] ^= F; state[6] ^= G; state[7] ^= H;
}

// 优化：在压缩函数中展开循环
void sm3_compress_opt1(uint32_t state[8], const uint32_t W[68], const uint32_t W1[64]) {
    uint32_t A = state[0], B = state[1], C = state[2], D = state[3];
    uint32_t E = state[4], F = state[5], G = state[6], H = state[7];

    for (int j = 0; j < 64; j += 4) {
        // 第1轮
        uint32_t SS1 = ROTL((ROTL(A, 12) + E + ROTL(T[j], j)), 7);
        uint32_t SS2 = SS1 ^ ROTL(A, 12);
        uint32_t TT1 = (j < 16 ? FF0(A, B, C) : FF1(A, B, C)) + D + SS2 + W1[j];
        uint32_t TT2 = (j < 16 ? GG0(E, F, G) : GG1(E, F, G)) + H + SS1 + W[j];
        D = C; C = ROTL(B, 9); B = A; A = TT1;
        H = G; G = ROTL(F, 19); F = E; E = P0(TT2);

        // 第2轮（j+1）... 类似处理
        // 第3轮（j+2）...
        // 第4轮（j+3）...
    }

    state[0] ^= A; // 更新状态...
}

// 预计算：预先计算 ROTL(T[j], j) 的值
static uint32_t T_rotl[64];
void init_T_rotl() {
    for (int j = 0; j < 64; j++) {
        T_rotl[j] = ROTL(T[j], j % 32);
    }
}

// 优化：使用AVX2处理4个消息块并行
// void sm3_compress_avx2(__m256i state[8], const __m256i W[68]) {
//     __m256i A = state[0], B = state[1], ...;
//     for (int j = 0; j < 64; j++) {
//         // 使用_mm256_xxx指令实现并行计算
//         __m256i SS1 = _mm256_rotl_epi32(...);
//         // ... 其他操作
//     }
//     state[0] = _mm256_xor_si256(state[0], A);
//     // ... 更新其他状态
// }

// 优化：合并消息扩展与压缩
// void sm3_process_block(uint32_t state[8], const uint8_t block[64]) {
//     uint32_t W[68], W1[64];
//     // 扩展与压缩合并，避免多次访问内存
//     for (int i = 0; i < 16; i++) {
//         W[i] = ...; // 直接计算
//     }
//     for (int i = 16; i < 68; i++) {
//         W[i] = ...; // 计算W
//         if (i >= 4) W1[i - 4] = W[i - 4] ^ W[i]; // 提前计算W1
//     }
//     // 立即调用压缩函数
//     sm3_compress_opt(state, W, W1);
// }

// 消除分支：使用掩码替代条件分支
#define FF(x, y, z, j) (((j) < 16) ? FF0(x, y, z) : FF1(x, y, z))
#define GG(x, y, z, j) (((j) < 16) ? GG0(x, y, z) : GG1(x, y, z))

// 或使用查表法（示例）
// static uint32_t FF_table[2] = {FF0, FF1}; // 伪代码，实际需封装函数指针

// 压缩函数中使用预计算值
// uint32_t SS1 = ROTL((ROTL(A, 12) + E + T_rotl[j]), 7); // 替换原计算
// SM3主函数
void sm3_hash(const uint8_t *msg, size_t len, uint8_t digest[32]) {
    // 初始向量
    uint32_t state[8] = {
        0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
        0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
    };

    // 消息填充
    uint8_t *padded_msg;
    size_t padded_len;
    sm3_pad(msg, len, &padded_msg, &padded_len);

    // 处理每个分组
    size_t blocks = padded_len / 64;
    for (size_t i = 0; i < blocks; i++) {
        uint32_t W[68], W1[64];
        sm3_expand(padded_msg + i * 64, W, W1);
        sm3_compress(state, W, W1);
    }

    // 输出大端序结果
    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state[i] & 0xFF;
    }

    free(padded_msg);
}