#include <stdio.h>
#include <stdint.h>

#define BLOCK_SIZE 16  

const uint32_t Sbox[] = {
    0xD6, 0x90, 0xE9, 0xFE, 0xCC, 0x5D, 0x11, 0x9A, 0x6F, 0x8B, 0xB3, 0x37, 0x7E, 0xF0, 0x93, 0xC5,
    0x77, 0xAD, 0xC7, 0x02, 0x1B, 0x93, 0x58, 0x25, 0xC5, 0xB9, 0xE2, 0xD0, 0x5A, 0xBB, 0x96, 0x94
};

const uint32_t FK[32] = {
    0xA3B1BAE9, 0x59DDA23E, 0x5C4DD124, 0x2F1BB5EA, 0xC9BC3CD2, 0x2883B8A3, 0x464860BD, 0x5141AB54,
    0x18A915CF, 0x2F2DF196, 0x31E55A31, 0xC095CE8E, 0x1DE0D795, 0x590415F9, 0x758FE5EE, 0xC55AD21B
};

uint32_t sbox_transform(uint32_t word) {
    uint32_t result = 0;
    for (int i = 0; i < 4; ++i) {
        uint8_t byte = (word >> (i * 8)) & 0xFF;
        result |= Sbox[byte] << (i * 8);
    }
    return result;
}

uint32_t t_table_transform(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3) {
    uint32_t y0 = sbox_transform(x0);
    uint32_t y1 = sbox_transform(x1);
    uint32_t y2 = sbox_transform(x2);
    uint32_t y3 = sbox_transform(x3);

    // 线性混合 + 轮密钥加
    uint32_t z0 = y0 ^ y1 ^ y2 ^ y3 ^ FK[0];
    uint32_t z1 = y1 ^ y2 ^ y3 ^ z0 ^ FK[1];
    uint32_t z2 = y2 ^ y3 ^ z0 ^ z1 ^ FK[2];
    uint32_t z3 = y3 ^ z0 ^ z1 ^ z2 ^ FK[3];

    return z0 ^ z1 ^ z2 ^ z3; 
}

// 执行 32 轮加密（模拟 SM4 轮函数）
void sm4_encrypt_rounds(uint32_t *state, const uint32_t *sk) {
    for (int i = 0; i < 32; ++i) {
        state[0] ^= sk[i];  

        // 使用 T 表变换更新其他状态字
        state[1] = t_table_transform(state[1], state[2], state[3], state[0]);
        state[2] ^= state[1];
        state[3] = t_table_transform(state[3], state[0], state[1], state[2]);
        state[0] ^= state[3];

        // 每两轮交换 state[1] 和 state[2]（模拟扩散）
        if (i % 2 == 0) {
            uint32_t temp = state[1];
            state[1] = state[2];
            state[2] = temp;
        }
    }

    state[0] ^= sk[32];
    state[1] ^= sk[33];
    state[2] ^= sk[34];
    state[3] ^= sk[35];  
}

// 密钥扩展（占位符）：从 128 位密钥生成 36 个轮密钥
void sm4_key_schedule(const uint8_t *key, uint32_t *rk) {
    for (int i = 0; i < 36; ++i) {
        rk[i] = (key[(i*4)%16] << 24) | (key[(i*4+1)%16] << 16) |
                (key[(i*4+2)%16] << 8) | key[(i*4+3)%16];
    }
}

// SM4 加密主函数
void sm4_encrypt(const uint8_t *plaintext, const uint8_t *key, uint8_t *ciphertext) {
    uint32_t state[4];  
    uint32_t rk[36];    

    // 将明文（16 字节）加载为 4 个大端 32 位字
    for (int i = 0; i < 4; ++i) {
        state[i] = (plaintext[i*4] << 24) | (plaintext[i*4+1] << 16) |
                   (plaintext[i*4+2] << 8) | plaintext[i*4+3];
    }

    sm4_key_schedule(key, rk);        
    sm4_encrypt_rounds(state, rk);    

    // 将加密后的状态写回密文（大端格式）
    for (int i = 0; i < 4; ++i) {
        ciphertext[i*4] = (state[i] >> 24) & 0xFF;
        ciphertext[i*4+1] = (state[i] >> 16) & 0xFF;
        ciphertext[i*4+2] = (state[i] >> 8) & 0xFF;
        ciphertext[i*4+3] = state[i] & 0xFF;
    }
}

int main() {
    uint8_t key[BLOCK_SIZE] = { 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t plaintext[BLOCK_SIZE] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    uint8_t ciphertext[BLOCK_SIZE];

    sm4_encrypt(plaintext, key, ciphertext);  

    printf("Ciphertext: ");
    for (int i = 0; i < BLOCK_SIZE; ++i) {
        printf("%02X ", ciphertext[i]);
    }
    printf("\n");

    return 0;
}