#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>

// ====================== SM3 哈希算法实现 ======================
#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static const uint32_t T[64] = {
    0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519,
    0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519, 0x79CC4519,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A,
    0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A, 0x7A879D8A
};

#define FF0(x, y, z) ((x) ^ (y) ^ (z))
#define GG0(x, y, z) ((x) ^ (y) ^ (z))
#define FF1(x, y, z) (((x) & (y)) | ((x) & (z)) | ((y) & (z)))
#define GG1(x, y, z) (((x) & (y)) | ((~(x)) & (z)))

#define P0(x) ((x) ^ ROTL(x, 9) ^ ROTL(x, 17))
#define P1(x) ((x) ^ ROTL(x, 15) ^ ROTL(x, 23))

void sm3_pad(const uint8_t *msg, size_t len, uint8_t **out, size_t *out_len) {
    size_t blocks = (len + 1 + 8 + 63) / 64;
    *out_len = blocks * 64;
    *out = (uint8_t *)malloc(*out_len);
    memset(*out, 0, *out_len);
    memcpy(*out, msg, len);
    (*out)[len] = 0x80;

    uint64_t bit_len = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) {
        (*out)[*out_len - 8 + i] = (bit_len >> (56 - i * 8)) & 0xFF;
    }
}

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

void sm3_hash(const uint8_t *msg, size_t len, uint8_t digest[32]) {
    uint32_t state[8] = {
        0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
        0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
    };

    uint8_t *padded_msg;
    size_t padded_len;
    sm3_pad(msg, len, &padded_msg, &padded_len);

    size_t blocks = padded_len / 64;
    for (size_t i = 0; i < blocks; i++) {
        uint32_t W[68], W1[64];
        sm3_expand(padded_msg + i * 64, W, W1);
        sm3_compress(state, W, W1);
    }

    for (int i = 0; i < 8; i++) {
        digest[i * 4] = (state[i] >> 24) & 0xFF;
        digest[i * 4 + 1] = (state[i] >> 16) & 0xFF;
        digest[i * 4 + 2] = (state[i] >> 8) & 0xFF;
        digest[i * 4 + 3] = state[i] & 0xFF;
    }

    free(padded_msg);
}

// ====================== Merkle 树实现 ======================

// Merkle树节点结构
typedef struct MerkleNode {
    uint8_t hash[32];          // 节点哈希值
    struct MerkleNode *left;   // 左子节点
    struct MerkleNode *right;  // 右子节点
    size_t start_index;        // 包含的叶子起始索引
    size_t end_index;          // 包含的叶子结束索引
} MerkleNode;

// 存在性证明结构
typedef struct {
    size_t index;              // 叶子索引
    uint8_t leaf_hash[32];     // 叶子哈希值
    uint8_t **sibling_hashes;  // 路径上的兄弟节点哈希
    int *is_right_sibling;     // 兄弟节点是否为右节点 (0=左,1=右)
    size_t path_length;        // 路径长度
} InclusionProof;

// 不存在性证明结构
typedef struct {
    size_t lower_bound;        // 下界叶子索引
    size_t upper_bound;        // 上界叶子索引
    InclusionProof *lower_proof; // 下界存在性证明
    InclusionProof *upper_proof; // 上界存在性证明
} ExclusionProof;

// 计算叶子节点的哈希（带RFC6962前缀）
void compute_leaf_hash(const uint8_t *data, size_t len, uint8_t hash[32]) {
    uint8_t input[len + 1];
    input[0] = 0x00; // RFC6962叶子节点前缀
    memcpy(input + 1, data, len);
    sm3_hash(input, len + 1, hash);
}

// 计算内部节点的哈希（带RFC6962前缀）
void compute_internal_hash(const uint8_t *left_hash, const uint8_t *right_hash, uint8_t parent_hash[32]) {
    uint8_t input[65];
    input[0] = 0x01; // RFC6962内部节点前缀
    
    // 如果右节点为空，则复制左节点
    if (right_hash == NULL) {
        memcpy(input + 1, left_hash, 32);
        memcpy(input + 33, left_hash, 32);
    } else {
        memcpy(input + 1, left_hash, 32);
        memcpy(input + 33, right_hash, 32);
    }
    
    sm3_hash(input, 65, parent_hash);
}

// 递归构建Merkle树
MerkleNode* build_merkle_tree_level(uint8_t **leaf_hashes, size_t start, size_t end) {
    MerkleNode *node = (MerkleNode*)malloc(sizeof(MerkleNode));
    node->start_index = start;
    node->end_index = end;
    
    // 叶子节点
    if (start == end) {
        memcpy(node->hash, leaf_hashes[start], 32);
        node->left = NULL;
        node->right = NULL;
        return node;
    }
    
    // 内部节点
    size_t mid = start + (end - start) / 2;
    node->left = build_merkle_tree_level(leaf_hashes, start, mid);
    node->right = build_merkle_tree_level(leaf_hashes, mid + 1, end);
    
    // 计算当前节点哈希
    compute_internal_hash(node->left->hash, 
                         (node->right ? node->right->hash : NULL), 
                         node->hash);
    
    return node;
}

// 构建完整的Merkle树
MerkleNode* build_merkle_tree(uint8_t **leaf_hashes, size_t leaf_count) {
    return build_merkle_tree_level(leaf_hashes, 0, leaf_count - 1);
}

// 生成存在性证明
InclusionProof* generate_inclusion_proof(MerkleNode *root, size_t index) {
    if (index < root->start_index || index > root->end_index) {
        return NULL; // 索引超出范围
    }
    
    InclusionProof *proof = (InclusionProof*)malloc(sizeof(InclusionProof));
    proof->index = index;
    
    // 存储叶子哈希
    proof->path_length = 0;
    size_t max_depth = 64; // 足够大的初始值
    proof->sibling_hashes = (uint8_t**)malloc(max_depth * sizeof(uint8_t*));
    proof->is_right_sibling = (int*)malloc(max_depth * sizeof(int));
    
    MerkleNode *current = root;
    MerkleNode *path[64]; // 存储路径节点
    int path_direction[64]; // 0=左,1=右
    size_t path_index = 0;
    
    // 遍历到叶子节点
    while (current->left || current->right) {
        path[path_index] = current;
        
        if (index <= current->left->end_index) {
            // 向左子树
            current = current->left;
            path_direction[path_index] = 0; // 当前是左节点
        } else {
            // 向右子树
            current = current->right;
            path_direction[path_index] = 1; // 当前是右节点
        }
        path_index++;
    }
    
    // 存储叶子哈希
    memcpy(proof->leaf_hash, current->hash, 32);
    
    // 回溯路径收集兄弟节点哈希
    for (int i = path_index - 1; i >= 0; i--) {
        MerkleNode *node = path[i];
        
        if (path_direction[i] == 0) {
            // 当前是左节点，兄弟是右节点
            if (node->right) {
                proof->sibling_hashes[proof->path_length] = (uint8_t*)malloc(32);
                memcpy(proof->sibling_hashes[proof->path_length], node->right->hash, 32);
                proof->is_right_sibling[proof->path_length] = 1;
            } else {
                // 如果没有右兄弟，使用左兄弟（树不平衡）
                proof->sibling_hashes[proof->path_length] = (uint8_t*)malloc(32);
                memcpy(proof->sibling_hashes[proof->path_length], node->left->hash, 32);
                proof->is_right_sibling[proof->path_length] = 0;
            }
        } else {
            // 当前是右节点，兄弟是左节点
            proof->sibling_hashes[proof->path_length] = (uint8_t*)malloc(32);
            memcpy(proof->sibling_hashes[proof->path_length], node->left->hash, 32);
            proof->is_right_sibling[proof->path_length] = 0;
        }
        
        proof->path_length++;
    }
    
    return proof;
}

// 验证存在性证明
int verify_inclusion(const uint8_t *root_hash, InclusionProof *proof) {
    uint8_t current_hash[32];
    memcpy(current_hash, proof->leaf_hash, 32);
    
    for (size_t i = 0; i < proof->path_length; i++) {
        uint8_t parent_hash[32];
        
        if (proof->is_right_sibling[i]) {
            // 兄弟是右节点，当前是左节点
            compute_internal_hash(current_hash, proof->sibling_hashes[i], parent_hash);
        } else {
            // 兄弟是左节点，当前是右节点
            compute_internal_hash(proof->sibling_hashes[i], current_hash, parent_hash);
        }
        
        memcpy(current_hash, parent_hash, 32);
    }
    
    return memcmp(current_hash, root_hash, 32) == 0;
}

// 生成不存在性证明
ExclusionProof* generate_exclusion_proof(MerkleNode *root, uint8_t *target_hash, uint8_t **leaf_hashes, size_t leaf_count) {
    // 在叶子哈希中查找目标位置（假设叶子已排序）
    size_t lower_bound = 0;
    size_t upper_bound = leaf_count - 1;
    int found = 0;
    
    // 二分查找目标位置
    while (lower_bound <= upper_bound) {
        size_t mid = lower_bound + (upper_bound - lower_bound) / 2;
        int cmp = memcmp(target_hash, leaf_hashes[mid], 32);
        
        if (cmp == 0) {
            // 目标存在，不能生成不存在证明
            return NULL;
        } else if (cmp < 0) {
            upper_bound = mid - 1;
        } else {
            lower_bound = mid + 1;
        }
    }
    
    // 目标不存在，确定边界
    ExclusionProof *proof = (ExclusionProof*)malloc(sizeof(ExclusionProof));
    
    if (lower_bound == 0) {
        // 目标小于所有叶子
        proof->lower_bound = 0;
        proof->upper_bound = 0;
    } else if (lower_bound >= leaf_count) {
        // 目标大于所有叶子
        proof->lower_bound = leaf_count - 1;
        proof->upper_bound = leaf_count - 1;
    } else {
        // 目标在两个叶子之间
        proof->lower_bound = lower_bound - 1;
        proof->upper_bound = lower_bound;
    }
    
    // 生成边界的存在性证明
    proof->lower_proof = generate_inclusion_proof(root, proof->lower_bound);
    proof->upper_proof = generate_inclusion_proof(root, proof->upper_bound);
    
    return proof;
}

// 验证不存在性证明
int verify_exclusion(const uint8_t *root_hash, ExclusionProof *proof, uint8_t *target_hash) {
    // 验证边界存在性
    if (!verify_inclusion(root_hash, proof->lower_proof) ||
        !verify_inclusion(root_hash, proof->upper_proof)) {
        return 0;
    }
    
    // 检查边界顺序
    if (proof->lower_bound == proof->upper_bound) {
        // 目标在边界之外
        if (proof->lower_bound == 0) {
            // 所有叶子都大于目标
            return memcmp(target_hash, proof->lower_proof->leaf_hash, 32) < 0;
        } else {
            // 所有叶子都小于目标
            return memcmp(target_hash, proof->lower_proof->leaf_hash, 32) > 0;
        }
    } else {
        // 目标在两个边界之间
        return (memcmp(proof->lower_proof->leaf_hash, target_hash, 32) < 0) &&
               (memcmp(target_hash, proof->upper_proof->leaf_hash, 32) < 0);
    }
}

// 释放Merkle树内存
void free_merkle_tree(MerkleNode *node) {
    if (!node) return;
    
    if (node->left) free_merkle_tree(node->left);
    if (node->right) free_merkle_tree(node->right);
    
    free(node);
}

// 释放存在性证明内存
void free_inclusion_proof(InclusionProof *proof) {
    if (!proof) return;
    
    for (size_t i = 0; i < proof->path_length; i++) {
        free(proof->sibling_hashes[i]);
    }
    
    free(proof->sibling_hashes);
    free(proof->is_right_sibling);
    free(proof);
}

// 释放不存在性证明内存
void free_exclusion_proof(ExclusionProof *proof) {
    if (!proof) return;
    
    if (proof->lower_proof) free_inclusion_proof(proof->lower_proof);
    if (proof->upper_proof) free_inclusion_proof(proof->upper_proof);
    
    free(proof);
}

// ====================== 测试与验证 ======================

// 生成随机数据
void generate_random_data(uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] = rand() % 256;
    }
}

// 打印哈希值
void print_hash(const uint8_t hash[32]) {
    for (int i = 0; i < 32; i++) {
        printf("%02x", hash[i]);
    }
    printf("\n");
}

// 测试函数
void test_merkle_tree(size_t leaf_count) {
    printf("===== 测试 Merkle 树 (%zu 个叶子节点) =====\n", leaf_count);
    
    // 生成叶子节点数据
    uint8_t **leaf_data = (uint8_t**)malloc(leaf_count * sizeof(uint8_t*));
    uint8_t **leaf_hashes = (uint8_t**)malloc(leaf_count * sizeof(uint8_t*));
    
    for (size_t i = 0; i < leaf_count; i++) {
        leaf_data[i] = (uint8_t*)malloc(32);
        leaf_hashes[i] = (uint8_t*)malloc(32);
        
        generate_random_data(leaf_data[i], 32);
        compute_leaf_hash(leaf_data[i], 32, leaf_hashes[i]);
    }
    
    // 构建Merkle树
    clock_t start = clock();
    MerkleNode *root = build_merkle_tree(leaf_hashes, leaf_count);
    clock_t end = clock();
    
    printf("Merkle树构建完成, 耗时: %.2f ms\n", (double)(end - start) * 1000 / CLOCKS_PER_SEC);
    printf("根哈希: ");
    print_hash(root->hash);
    
    // 测试存在性证明
    size_t test_index = rand() % leaf_count;
    start = clock();
    InclusionProof *inc_proof = generate_inclusion_proof(root, test_index);
    end = clock();
    
    printf("\n存在性证明生成 (索引 %zu): %.2f ms\n", test_index, (double)(end - start) * 1000 / CLOCKS_PER_SEC);
    
    start = clock();
    int valid = verify_inclusion(root->hash, inc_proof);
    end = clock();
    
    printf("存在性证明验证: %s, 耗时: %.2f ms\n", valid ? "成功" : "失败", 
           (double)(end - start) * 1000 / CLOCKS_PER_SEC);
    
    // 测试不存在性证明
    uint8_t target_hash[32];
    generate_random_data(target_hash, 32);
    
    start = clock();
    ExclusionProof *exc_proof = generate_exclusion_proof(root, target_hash, leaf_hashes, leaf_count);
    end = clock();
    
    if (exc_proof) {
        printf("\n不存在性证明生成: %.2f ms\n", (double)(end - start) * 1000 / CLOCKS_PER_SEC);
        printf("目标位置: [%zu, %zu]\n", exc_proof->lower_bound, exc_proof->upper_bound);
        
        start = clock();
        valid = verify_exclusion(root->hash, exc_proof, target_hash);
        end = clock();
        
        printf("不存在性证明验证: %s, 耗时: %.2f ms\n", valid ? "成功" : "失败", 
               (double)(end - start) * 1000 / CLOCKS_PER_SEC);
        
        free_exclusion_proof(exc_proof);
    } else {
        printf("\n目标哈希存在，无法生成不存在证明\n");
    }
    
    // 清理内存
    free_inclusion_proof(inc_proof);
    free_merkle_tree(root);
    
    for (size_t i = 0; i < leaf_count; i++) {
        free(leaf_data[i]);
        free(leaf_hashes[i]);
    }
    
    free(leaf_data);
    free(leaf_hashes);
}

int main() {
    srand(time(NULL)); // 初始化随机种子
    
    // 测试不同规模的Merkle树
    test_merkle_tree(10);     // 10个叶子节点
    test_merkle_tree(100);    // 100个叶子节点
    test_merkle_tree(1000);   // 1000个叶子节点
    test_merkle_tree(10000);  // 10,000个叶子节点
    test_merkle_tree(100000); // 100,000个叶子节点
    
    return 0;
}