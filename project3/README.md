### poseidon2 哈希算法

概述：这个 Circom 电路实现了 Poseidon2 哈希算法，采用 (n, t, d) = (256, 2, 5) 参数配置，主要特点如下：

1. **分层设计**：

   - `FullRound`：完整轮处理（所有元素应用 S-box）
   - `PartialRound`：部分轮处理（仅第一个元素应用 S-box）
   - `Poseidon2`：主哈希函数（整合所有轮次处理）
   - `main`：Groth16 入口电路

2. **关键参数**：
   - 状态大小：t = 2
   - 完整轮：R_F = 8（前 4 + 后 4）
   - 部分轮：R_P = 56
   - 总轮数：64
   - S-box：x⁵ = (x²)²·x

#### 组件说明

1. **FullRound 模板**（完整轮）：

   ```circom
   template FullRound(round_constants, mds) {
     // 添加轮常数
     after_arc[i] = state_in[i] + RC[i]

     // S-box (所有元素)
     after_sbox[i] = (after_arc[i]²)² * after_arc[i]

     // MDS 矩阵混合
     state_out = MDS × after_sbox
   }
   ```

   - 每轮约束数：≈6（两个 S-box 各 3 约束）

2. **PartialRound 模板**（部分轮）：

   ```circom
   template PartialRound(round_constants, mds) {
     // 添加轮常数（同上）

     // 仅第一个元素应用 S-box
     after_sbox[0] = (after_arc[0]²)² * after_arc[0]
     after_sbox[1] = after_arc[1]  // 直通

     // MDS 矩阵混合（同上）
   }
   ```

   - 每轮约束数：≈4（一个 S-box 3 约束 + 直通）

3. **Poseidon2 主模板**：

   ```circom
   template Poseidon2() {
     // 初始化状态
     state[0] = [in, 0]

     // 轮次处理
     for (r=0 to 3):       // 前4完整轮
        state[r+1] = FullRound(state[r])

     for (r=4 to 59):      // 56部分轮
        state[r+1] = PartialRound(state[r])

     for (r=60 to 63):     // 后4完整轮
        state[r+1] = FullRound(state[r])

     // 输出哈希值
     out = state[64][0]
   }
   ```

   - 总约束数 ≈ 4×6 + 56×4 + 4×6 = 272 约束

4. **Groth16 入口**：
   ```circom
   component main {public [hash]} = Poseidon2();
   ```
   - 私有输入：`preimage`
   - 公开输出：`hash`

#### 参数配置要求

1. **MDS 矩阵**：

   ```circom
   var MDS = [
     [m00, m01],  // 需替换为安全参数
     [m10, m11]   // 如：0x1234...
   ];
   ```

2. **轮常数**：
   ```circom
   var round_constants[128] = [
     rc0_0, rc0_1, // 第0轮
     rc1_0, rc1_1, // 第1轮
     ...           // 共128个值
   ];
   ```

#### 安全参数获取

必须使用标准参数：

```bash
# 从官方库获取参数
git clone https://github.com/HorizenLabs/poseidon2
```

推荐参数来源：

1. 轮常数：`params/poseidon2_256_2_5/rc.txt`
2. MDS 矩阵：`params/poseidon2_256_2_5/mds.txt`

#### 使用流程

1. **编译电路**：

   ```bash
   circom poseidon2.circom --r1cs --wasm --sym -o build
   ```

2. **生成证明**：

   ```bash
   # 生成 witness
   node build/poseidon2_js/generate_witness.js \
        build/poseidon2_js/poseidon2.wasm \
        input.json \
        witness.wtns

   # Groth16 证明
   snarkjs groth16 prove circuit.zkey witness.wtns proof.json public.json
   ```

3. **输入文件示例** (`input.json`)：

   ```json
   {
     "in": "123456789" // 私有输入（原像）
   }
   ```

4. **验证证明**：
   ```bash
   snarkjs groth16 verify verification_key.json public.json proof.json
   ```

#### 性能优化

1. **S-box 约束优化**：

   - x⁵ 计算分解为 `(x²)²·x`（3 约束）
   - 比直接 `x*x*x*x*x`（4 约束）更高效

2. **内存管理**：
   - 逐轮声明状态信号（避免大型数组）
   - 编译时展开循环（无运行时开销）
