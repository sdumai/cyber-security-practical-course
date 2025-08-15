# Google Password Checkup 协议实现

## 1. 概述

该项目实现 Google Password Checkup 协议的简化版本，基于刘巍然老师报告中提到的论文（[https://eprint.iacr.org/2019/723.pdf](https://eprint.iacr.org/2019/723.pdf)）第 3.1 节（Figure 2）描述的协议。该协议允许客户端在不泄露实际密码的情况下，检查其凭证是否存在于已知泄露凭证数据库中。

## 2. 协议原理

### 2.1 核心思想

协议采用多方安全计算技术，通过以下方式保护用户隐私：

- 客户端凭证不会以明文形式传输
- 服务器无法直接获取客户端凭证信息
- 只有凭证确实存在于泄露数据库中时才会被检测到

### 2.2 协议流程

1. **初始化阶段**：

   - 选择大素数 p 和生成元 g₁, g₂
   - 预计算泄露凭证的密钥份额

2. **客户端阶段**：

   - 对凭证进行加密处理
   - 生成随机数保护查询隐私

3. **Helper 阶段**：

   - 处理部分计算，不直接接触凭证信息

4. **Server 阶段**：
   - 完成最终匹配计算
   - 返回凭证是否泄露的结果

## 3. 代码结构

### 3.1 主要类：`GooglePasswordCheckup`

#### 初始化方法 `__init__(self, bits=32)`

- 功能：初始化协议参数
- 参数：
  - `bits`：素数位数（默认为 32 位，实际应用应更大）
- 生成：
  - 素数 p
  - 生成元 g₁, g₂
  - 泄露凭证存储列表
  - 密钥份额列表

#### 辅助方法 `_generate_prime(self)`

- 功能：生成指定位数的素数
- 返回值：一个大素数

#### 哈希函数

1. `H1(self, u, p)`

   - 功能：将凭证映射到 Zₚ\* 中的元素
   - 参数：用户名 u，密码 p
   - 返回值：整数 ∈ [1, p-1]

2. `H2(self, u, p)`

   - 功能：将凭证映射为定长字符串（用于最终比对）
   - 返回值：8 字符十六进制字符串

3. `H3(self, t, k)`
   - 功能：将两个群元素映射为定长字符串
   - 返回值：8 字符十六进制字符串（与 H2 长度一致）

#### 数据库操作方法 `add_breached_credential(self, u, p)`

- 功能：添加泄露凭证到数据库
- 参数：用户名 u，密码 p
- 内部操作：
  - 计算 c_i = H1(u,p)
  - 生成随机数 a_i
  - 计算 b_i = c_i \* a_i^{-1} mod p
  - 存储相关数据

#### 协议阶段方法

1. `client_step(self, u, p)`

   - 功能：执行客户端协议步骤
   - 返回值：
     - (s, t)：发送给 Helper 的数据
     - (d, y)：发送给 Server 的数据

2. `helper_step(self, t)`

   - 功能：执行 Helper 协议步骤
   - 参数：来自客户端的 t
   - 返回值：t_i 列表（发送给 Server）

3. `server_step(self, d, y, t_i_list)`
   - 功能：执行 Server 协议步骤
   - 返回值：
     - 布尔值：是否泄露
     - 整数：泄露凭证索引（-1 表示未泄露）

### 3.2 测试函数 `test_protocol()`

- 功能：完整测试协议流程
- 测试用例：
  - 已知泄露凭证（应检测到泄露）
  - 安全凭证（应无泄露）

## 4. 使用方法

### 4.1 初始化

```python
gpc = GooglePasswordCheckup(bits=16)  # 实际应用应使用更大位数
```

### 4.2 添加泄露凭证

```python
gpc.add_breached_credential("user1", "password1")
gpc.add_breached_credential("user2", "password2")
```

### 4.3 执行协议检查

```python
# 客户端步骤
(s, t), (d, y) = gpc.client_step("user1", "password1")

# Helper步骤
t_i_list = gpc.helper_step(t)

# Server步骤
is_breached, index = gpc.server_step(d, y, t_i_list)
```

### 4.4 运行测试

```python
if __name__ == "__main__":
    test_protocol()
```

## 5. 测试结果说明

测试输出示例：

```
Generated prime p = 50753
Generator g1 = g2 = 5
Added 2 breached credentials

Test: (user1, password1)
Expected breached: True, Actual: False (Index: -1)
Result: FAILED

Test: (user2, password2)
Expected breached: True, Actual: False (Index: -1)
Result: FAILED

Test: (safe_user, safe_pass)
Expected breached: False, Actual: False (Index: -1)
Result: PASSED
```
