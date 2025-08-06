## 一、SM4 加密算法说明

### 1. 概述

SM4 是是一种分组对称加密算法，分组长度为 128 位（16 字节），密钥长度也为 128 位。本代码实现了 SM4 的基本加密功能。

### 2. 代码结构

#### 2.1 常量定义

- `BLOCK_SIZE`: 定义分组大小为 16 字节
- `Sbox`: 256 字节的 S 盒置换表
- `FK`: 系统参数，用于密钥扩展

#### 2.2 核心函数

##### 2.2.1 S 盒变换 (`sbox_transform`)

```c
uint32_t sbox_transform(uint32_t word)
```

- 功能：对 32 位字进行 S 盒置换
- 实现：
  1. 将 32 位字拆分为 4 个字节
  2. 对每个字节进行 S 盒查表替换
  3. 重新组合为 32 位字返回

##### 2.2.2 T 变换 (`t_table_transform`)

```c
uint32_t t_table_transform(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3)
```

- 功能：实现 SM4 的合成置换 T
- 实现：
  1. 对 4 个输入字分别进行 S 盒变换
  2. 进行线性变换 L
  3. 与系统参数 FK 进行异或

##### 2.2.3 加密轮函数 (`sm4_encrypt_rounds`)

```c
void sm4_encrypt_rounds(uint32_t *state, const uint32_t *sk)
```

- 功能：执行 32 轮加密运算
- 实现：
  1. 每轮对状态字进行异或和 T 变换
  2. 每两轮交换中间状态字
  3. 最后进行反序变换

##### 2.2.4 密钥扩展 (`sm4_key_schedule`)

```c
void sm4_key_schedule(const uint8_t *key, uint32_t *rk)
```

- 功能：从初始密钥生成轮密钥
- 实现：
  1. 将 128 位密钥转换为 4 个 32 位字
  2. 通过变换生成 36 个轮密钥

##### 2.2.5 加密主函数 (`sm4_encrypt`)

```c
void sm4_encrypt(const uint8_t *plaintext, const uint8_t *key, uint8_t *ciphertext)
```

- 功能：完整的 SM4 加密流程
- 实现：
  1. 将明文分组转换为状态字
  2. 执行密钥扩展
  3. 执行 32 轮加密
  4. 将最终状态转换为密文输出

#### 2.3 测试主函数

```c
int main()
```

- 功能：测试加密功能
- 实现：
  1. 定义测试密钥和明文
  2. 调用 sm4_encrypt 进行加密
  3. 输出密文结果

### 3. 算法流程

1. **密钥扩展**：

   - 将初始密钥转换为 4 个 32 位字
   - 通过变换生成 36 个轮密钥

2. **加密过程**：

   - 将明文分组转换为 4 个状态字(X0,X1,X2,X3)
   - 执行 32 轮迭代运算：
     - 每轮对状态字进行异或和 T 变换
     - 每两轮交换中间状态字
   - 最后进行反序变换输出密文

3. **核心变换**：
   - S 盒变换：非线性字节替换
   - T 变换：合成置换，包含 S 盒变换和线性变换

### 4. 使用方法

1. 包含必要的头文件：

   ```c
   #include <stdio.h>
   #include <stdint.h>
   ```

2. 定义明文和密钥：

   ```c
   uint8_t key[16] = {...};
   uint8_t plaintext[16] = {...};
   uint8_t ciphertext[16];
   ```

3. 调用加密函数：

   ```c
   sm4_encrypt(plaintext, key, ciphertext);
   ```

4. 输出结果：
   ```c
   for(int i=0; i<16; i++) {
       printf("%02X ", ciphertext[i]);
   }
   ```

## 二、SM4-GCM 优化算法说明

### 1. 概述

基于 SM4 分组密码算法实现 GCM(Galois/Counter Mode)工作模式的优化。GCM 是一种提供认证加密的工作模式，结合了计数器模式(CTR)的加密和 Galois 模式的认证。

### 2. SM4-GCM 实现架构

#### 2.1 主要组件

1. **SM4 加密核心**：实现基本的 SM4 分组加密算法
2. **GCM 模式实现**：包含 GHASH 乘法和计数器模式加密
3. **认证加密接口**：提供完整的 GCM 加密/解密接口

#### 2.2 数据流程

```
明文/密文 → CTR模式加密/解密 (使用SM4)
          ↘
附加认证数据(AAD) → GHASH乘法 → 生成认证标签
```

### 3. 关键实现细节

#### 3.1 SM4 核心实现

1. **S 盒变换**：使用预定义的 S 盒进行非线性变换
2. **轮函数**：实现 32 轮加密运算
3. **密钥扩展**：从主密钥生成轮密钥(当前实现为简化版本)

#### 3.2 GCM 模式实现

##### 3.2.1 GHASH 乘法

- 实现 GF(2^128)上的乘法运算
- 使用查表法优化乘法运算
- 处理任意长度的输入数据

##### 3.2.2 计数器模式

- 初始化计数器 J0
- 生成密钥流并与明文/密文异或
- 计数器递增规则实现

##### 3.2.3 认证标签生成

- 处理附加认证数据(AAD)
- 处理加密/解密数据
- 组合生成最终认证标签

### 4. 优化策略

#### 4.1 查表优化

- 预计算 GHASH 乘法表，减少运行时计算量
- 优化 S 盒访问模式，提高缓存命中率

#### 4.2 并行处理

- 利用现代 CPU 的 SIMD 指令并行处理多个块
- 多线程处理独立的数据块

#### 4.3 内存访问优化

- 对齐内存访问，提高内存吞吐量
- 减少不必要的内存拷贝

### 5. 接口设计

#### 5.1 加密接口

```c
void gcm_encrypt(
    const uint8_t *plaintext,
    size_t plaintext_len,
    const uint8_t *key,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    uint8_t *ciphertext,
    uint8_t *auth_tag
);
```

#### 5.2 解密接口

```c
void gcm_decrypt(
    const uint8_t *ciphertext,
    size_t ciphertext_len,
    const uint8_t *key,
    const uint8_t *iv,
    size_t iv_len,
    const uint8_t *aad,
    size_t aad_len,
    uint8_t *plaintext,
    uint8_t *auth_tag
);
```
