pragma circom 2.1.6;

// 完整轮操作 
template FullRound(round_constants, mds) {
    signal input state_in[2];
    signal output state_out[2];
    
    // AddRoundConstants
    signal after_arc[2];
    after_arc[0] <== state_in[0] + round_constants[0];
    after_arc[1] <== state_in[1] + round_constants[1];
    
    // 优化S-box计算: x^5 = (x^2)^2 * x
    signal s0_sq <== after_arc[0] * after_arc[0];
    signal s0_quad <== s0_sq * s0_sq;
    signal after_sbox0 <== s0_quad * after_arc[0];
    
    signal s1_sq <== after_arc[1] * after_arc[1];
    signal s1_quad <== s1_sq * s1_sq;
    signal after_sbox1 <== s1_quad * after_arc[1];
    
    // MixLayer (矩阵乘法)
    state_out[0] <== mds[0][0]*after_sbox0 + mds[0][1]*after_sbox1;
    state_out[1] <== mds[1][0]*after_sbox0 + mds[1][1]*after_sbox1;
}

// 部分轮操作
template PartialRound(round_constants, mds) {
    signal input state_in[2];
    signal output state_out[2];
    
    // AddRoundConstants
    signal after_arc[2];
    after_arc[0] <== state_in[0] + round_constants[0];
    after_arc[1] <== state_in[1] + round_constants[1];
    
    // 只对第一个元素应用S-box
    signal s0_sq <== after_arc[0] * after_arc[0];
    signal s0_quad <== s0_sq * s0_sq;
    signal after_sbox0 <== s0_quad * after_arc[0];
    
    // 第二个元素直接传递
    signal after_sbox1 <== after_arc[1];
    
    // MixLayer
    state_out[0] <== mds[0][0]*after_sbox0 + mds[0][1]*after_sbox1;
    state_out[1] <== mds[1][0]*after_sbox0 + mds[1][1]*after_sbox1;
}

// 主Poseidon2模板
template Poseidon2() {
    // 参数: (n, t, d) = (256, 2, 5)
    // 轮数配置 (R_F = 8, R_P = 56)
    var R_F = 8;        // 完整轮数
    var R_P = 56;       // 部分轮数
    var ROUNDS = R_F + R_P;
    
    var MDS = [
        [ 0x1234, 0x5678],
        [ 0x9abc, 0xdef0]
    ];
    
    var round_constants[128];
    component poseidon = Poseidon(2, 8, 57);
    for (var i = 0; i < 128; i++) {
        round_constants[i] = poseidon.C[i];
    }
    
    // 输入输出定义
    signal input in;
    signal output out;
    
    // 状态数组: [轮次][状态索引]
    signal state[0][0] <== in;   // 初始状态[0]
    signal state[0][1] <== 0;    // 初始状态[1]
    
    var rc_index = 0;
    
    // 前4个完整轮
    for (var r = 0; r < R_F/2; r++) {
        component full = FullRound(
            [round_constants[rc_index], round_constants[rc_index + 1]], 
            MDS
        );
        full.state_in[0] <== state[r][0];
        full.state_in[1] <== state[r][1];
        
        // 创建下一轮状态信号
        signal state[r+1][0];
        signal state[r+1][1];
        state[r+1][0] <== full.state_out[0];
        state[r+1][1] <== full.state_out[1];
        
        rc_index += 2;
    }
    
    // 56个部分轮
    for (var r = R_F/2; r < R_F/2 + R_P; r++) {
        component partial = PartialRound(
            [round_constants[rc_index], round_constants[rc_index + 1]], 
            MDS
        );
        partial.state_in[0] <== state[r][0];
        partial.state_in[1] <== state[r][1];
        
        signal state[r+1][0];
        signal state[r+1][1];
        state[r+1][0] <== partial.state_out[0];
        state[r+1][1] <== partial.state_out[1];
        
        rc_index += 2;
    }
    
    // 后4个完整轮
    for (var r = R_F/2 + R_P; r < ROUNDS; r++) {
        component full = FullRound(
            [round_constants[rc_index], round_constants[rc_index + 1]], 
            MDS
        );
        full.state_in[0] <== state[r][0];
        full.state_in[1] <== state[r][1];
        
        signal state[r+1][0];
        signal state[r+1][1];
        state[r+1][0] <== full.state_out[0];
        state[r+1][1] <== full.state_out[1];
        
        rc_index += 2;
    }
    
    // 输出最终状态的第一个元素
    out <== state[ROUNDS][0];
}

// Groth16证明系统入口
component main {public [hash]} = Poseidon2();