#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

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
        uint32_t TT1 = (j < 16) ? FF0(A, B, C) + D + SS2 + W1[j] : 
                                 FF1(A, B, C) + D + SS2 + W1[j];
        uint32_t TT2 = (j < 16) ? GG0(E, F, G) + H + SS1 + W[j] : 
                                 GG1(E, F, G) + H + SS1 + W[j];
        D = C; 
        C = ROTL(B, 9); 
        B = A; 
        A = TT1;
        H = G; 
        G = ROTL(F, 19); 
        F = E; 
        E = P0(TT2);
    }

    state[0] ^= A; 
    state[1] ^= B; 
    state[2] ^= C; 
    state[3] ^= D;
    state[4] ^= E; 
    state[5] ^= F; 
    state[6] ^= G; 
    state[7] ^= H;
}

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

// 从自定义IV计算哈希
void sm3_hash_from_iv(const uint32_t iv[8], 
                     const uint8_t *msg, 
                     size_t len, 
                     uint8_t digest[32]) {
    uint32_t state[8];
    memcpy(state, iv, sizeof(uint32_t) * 8);

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

    // 输出结果
    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state[i] & 0xFF;
    }

    free(padded_msg);
}

// 长度扩展攻击验证
int length_extension_attack() {
    // 原始消息和哈希
    const char *original_msg = "secret";
    uint8_t orig_digest[32];
    sm3_hash((uint8_t *)original_msg, strlen(original_msg), orig_digest);
    
    printf("原始消息: '%s'\n", original_msg);
    printf("原始哈希: ");
    for (int i = 0; i < 32; i++) printf("%02x", orig_digest[i]);
    printf("\n");
    
    // 提取原始状态作为新IV
    uint32_t new_iv[8];
    for (int i = 0; i < 8; i++) {
        new_iv[i] = ((uint32_t)orig_digest[i*4] << 24) |
                    ((uint32_t)orig_digest[i*4+1] << 16) |
                    ((uint32_t)orig_digest[i*4+2] << 8) |
                    orig_digest[i*4+3];
    }
    
    // 构造扩展消息
    const char *extension = "malicious";
    size_t orig_len = strlen(original_msg);
    
    // 计算原始填充
    size_t pad_len = 64 - (orig_len % 64);
    if (pad_len < 9) pad_len += 64;
    
    // 构造新消息: 原始消息 + 填充 + 扩展
    size_t new_len = orig_len + pad_len + strlen(extension);
    uint8_t *new_msg = malloc(new_len);
    
    // 填充格式: 0x80 + 0x00... + 原始长度
    memcpy(new_msg, original_msg, orig_len);
    new_msg[orig_len] = 0x80;
    memset(new_msg + orig_len + 1, 0, pad_len - 1 - 8);
    
    // 添加原始长度（大端序）
    uint64_t bit_len = orig_len * 8;
    for (int i = 0; i < 8; i++) {
        new_msg[orig_len + pad_len - 8 + i] = (bit_len >> (56 - i*8)) & 0xFF;
    }
    
    // 添加扩展内容
    memcpy(new_msg + orig_len + pad_len, extension, strlen(extension));
    
    // 计算攻击结果
    uint8_t attack_digest[32];
    sm3_hash_from_iv(new_iv, (uint8_t *)extension, strlen(extension), attack_digest);
    
    // 计算真实结果
    uint8_t real_digest[32];
    sm3_hash(new_msg, new_len, real_digest);
    
    // 输出结果
    printf("\n扩展消息: 'secret' + padding + 'malicious'\n");
    printf("真实哈希: ");
    for (int i = 0; i < 32; i++) printf("%02x", real_digest[i]);
    printf("\n攻击哈希: ");
    for (int i = 0; i < 32; i++) printf("%02x", attack_digest[i]);
    printf("\n");
    
    // 比较结果
    int success = memcmp(attack_digest, real_digest, 32) == 0;
    
    free(new_msg);
    return success;
}

int main() {
    printf("SM3 长度扩展攻击验证\n");
    printf("======================================\n");
    
    if (length_extension_attack()) {
        printf("\n攻击成功! SM3 易受长度扩展攻击\n");
    } else {
        printf("\n攻击失败! SM3 抵抗了长度扩展攻击\n");
    }
    
    printf("\n防御建议: 使用 HMAC-SM3 或截断哈希值\n");
    return 0;
}