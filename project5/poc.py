from gmssl import sm2, func
import secrets

# 初始化SM2曲线参数
p = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF
a = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC
b = 0x28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93
n = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123
Gx = 0x32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7
Gy = 0xBC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0

class SM2FixedK(sm2.CryptSM2):
    def sign_fixed_k(self, data, k_hex):
        e = self.sm3_z(func.bytes_to_list(data))
        e_int = int.from_bytes(e, 'big')
        k = int(k_hex, 16)
        
        # 计算点(x1, y1) = k·G
        point = self._kg(k, self.ecc_table['g'])
        x1 = int(point[2:2+64], 16)
        r = (e_int + x1) % int(self.ecc_table['n'], 16)
        s = (self._inverse(1 + self.private_key, int(self.ecc_table['n'], 16)) * 
             (k - r * self.private_key) % int(self.ecc_table['n'], 16)
        return (r, s)

# 生成固定k
fixed_k = secrets.randbelow(n)
k_hex = hex(fixed_k)[2:].zfill(64)

# 初始化密钥对
private_key = "00B9AB0B828FF68872F21A837FC303668428DEA11DCD1B24429D0C99E24EED83D5"
public_key = "B9C9A6E04E9C91F7BA880429273747D7EF5DDEB0BB2FF6317EB00BEF331A83081A6994B8993F3F5D6EADDDB81872266C87C018FB4162F5AF347B483E24620207"

# 创建签名实例
sm2_fixedk = SM2FixedK(
    private_key=private_key,
    public_key=public_key,
    ecc_table=sm2.default_ecc_table
)

# 对两个不同消息使用相同k签名
msg1 = b"Critical message 1"
msg2 = b"Critical message 2"

(r1, s1) = sm2_fixedk.sign_fixed_k(msg1, k_hex)
(r2, s2) = sm2_fixedk.sign_fixed_k(msg2, k_hex)

# 计算哈希值
def calc_e(msg):
    z = sm2_fixedk.sm3_z(func.bytes_to_list(msg))
    return int.from_bytes(z, 'big')

e1 = calc_e(msg1)
e2 = calc_e(msg2)

# 验证签名有效性
assert sm2_fixedk.verify(s1, r1, msg1)  # 应为True
assert sm2_fixedk.verify(s2, r2, msg2)  # 应为True

# 推导私钥 (核心攻击)
delta_r = (r1 - r2) % n
delta_s = (s1 - s2) % n
denominator = (delta_r + s2 - s1) % n

d_recovered = (delta_s * pow(denominator, -1, n)) % n

# 验证推导结果
print(f"真实私钥: {int(private_key, 16)}")
print(f"推导私钥: {d_recovered}")
assert d_recovered == int(private_key, 16)