import random
import hashlib
import time
import multiprocessing
from functools import lru_cache

# SM2 椭圆曲线参数 (国密标准)
P = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFF
A = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF00000000FFFFFFFFFFFFFFFC
B = 0x28E9FA9E9D9F5E344D5A9E4BCF6509A7F39789F515AB8F92DDBCBD414D940E93
N = 0xFFFFFFFEFFFFFFFFFFFFFFFFFFFFFFFF7203DF6B21C6052B53BBF40939D54123
Gx = 0x32C4AE2C1F1981195F9904466A39C9948FE30BBFF2660BE1715A4589334C74C7
Gy = 0xBC3736A2F4F6779C59BDCEE36B692153D0A9877CC62A474002DF32E52139F0A0

# 椭圆曲线点类
class ECPoint:
    def __init__(self, x, y, is_infinity=False):
        self.x = x
        self.y = y
        self.is_infinity = is_infinity
    
    def __eq__(self, other):
        if self.is_infinity and other.is_infinity:
            return True
        return self.x == other.x and self.y == other.y and not self.is_infinity and not other.is_infinity
    
    def __repr__(self):
        if self.is_infinity:
            return "Point(Infinity)"
        return f"Point(0x{self.x:064x}, 0x{self.y:064x})"

# 无穷远点
INFINITY = ECPoint(0, 0, is_infinity=True)
# 基点
G = ECPoint(Gx, Gy)

# 有限域运算
def mod_inv(a, modulus=P):
    """扩展欧几里得算法求模逆"""
    if a == 0:
        return 0
    lm, hm = 1, 0
    low, high = a % modulus, modulus
    while low > 1:
        ratio = high // low
        nm = hm - lm * ratio
        new = high - low * ratio
        hm, lm = lm, nm
        high, low = low, new
    return lm % modulus

def mod_add(a, b, modulus=P):
    """模加法"""
    return (a + b) % modulus

def mod_sub(a, b, modulus=P):
    """模减法"""
    return (a - b) % modulus

def mod_mul(a, b, modulus=P):
    """模乘法"""
    return (a * b) % modulus

def mod_div(a, b, modulus=P):
    """模除法"""
    return mod_mul(a, mod_inv(b, modulus), modulus)

# 椭圆曲线点运算
def point_add(p, q):
    """椭圆曲线点加法"""
    if p.is_infinity:
        return q
    if q.is_infinity:
        return p
    if p.x == q.x and p.y != q.y:
        return INFINITY
    
    if p == q:
        # 点倍乘
        lam = mod_div(
            mod_add(mod_mul(3, mod_mul(p.x, p.x)), mod_mul(2, p.y)),
            P
        )
    else:
        # 点加法
        lam = mod_div(
            mod_sub(q.y, p.y),
            mod_sub(q.x, p.x),
            P
        )
    
    x3 = mod_sub(mod_sub(mod_mul(lam, lam), p.x), q.x)
    y3 = mod_sub(mod_mul(lam, mod_sub(p.x, x3)), p.y)
    
    return ECPoint(x3, y3)

def point_mul(k, point):
    """椭圆曲线点乘 - 基础实现 (double-and-add)"""
    result = INFINITY
    addend = point
    
    while k:
        if k & 1:
            result = point_add(result, addend)
        addend = point_add(addend, addend)
        k >>= 1
    
    return result

# 预计算点表优化
@lru_cache(maxsize=128)
def precompute_point_table(point, window_size=4):
    """预计算点表用于滑动窗口算法"""
    table = [INFINITY] * (1 << window_size)
    table[0] = INFINITY
    table[1] = point
    
    # 计算2的幂次点
    for i in range(2, 1 << window_size):
        table[i] = point_add(table[i-1], point)
    
    return table

def point_mul_window(k, point, window_size=4):
    """使用滑动窗口优化的点乘算法"""
    table = precompute_point_table(point, window_size)
    result = INFINITY
    i = 0
    n = k.bit_length()
    
    while i < n:
        if not (k >> i) & 1:
            i += 1
            continue
        
        # 提取窗口
        window = 0
        for j in range(window_size):
            if i + j < n:
                window |= ((k >> (i + j)) & 1) << j
        
        # 跳过全零窗口
        if window == 0:
            i += window_size
            continue
        
        # 添加窗口点
        result = point_add(result, table[window])
        i += window_size
    
    return result

# Jacobian坐标优化
class JacobianPoint:
    def __init__(self, x, y, z):
        self.x = x
        self.y = y
        self.z = z

def to_jacobian(p):
    """仿射坐标转Jacobian坐标"""
    return JacobianPoint(p.x, p.y, 1) if not p.is_infinity else JacobianPoint(0, 0, 0)

def from_jacobian(jp):
    """Jacobian坐标转仿射坐标"""
    if jp.z == 0:
        return INFINITY
    
    z_inv = mod_inv(jp.z)
    z_inv_sq = mod_mul(z_inv, z_inv)
    x = mod_mul(jp.x, z_inv_sq)
    y = mod_mul(jp.y, mod_mul(z_inv, z_inv_sq))
    
    return ECPoint(x, y)

def jacobian_point_add(p, q):
    """Jacobian坐标下的点加法"""
    if p.z == 0:
        return q
    if q.z == 0:
        return p
    
    # 计算中间量
    u1 = mod_mul(p.x, mod_mul(q.z, q.z))
    u2 = mod_mul(q.x, mod_mul(p.z, p.z))
    s1 = mod_mul(p.y, mod_mul(q.z, mod_mul(q.z, q.z)))
    s2 = mod_mul(q.y, mod_mul(p.z, mod_mul(p.z, p.z)))
    
    if u1 == u2:
        if s1 != s2:
            return JacobianPoint(0, 0, 0)  # 无穷远点
        return jacobian_point_double(p)
    
    h = mod_sub(u2, u1)
    r = mod_sub(s2, s1)
    h_sq = mod_mul(h, h)
    h_cu = mod_mul(h_sq, h)
    
    x3 = mod_sub(mod_sub(mod_mul(r, r), h_cu), mod_mul(2, mod_mul(u1, h_sq)))
    y3 = mod_sub(mod_mul(r, mod_sub(mod_mul(u1, h_sq), x3)), mod_mul(s1, h_cu))
    z3 = mod_mul(h, mod_mul(p.z, q.z))
    
    return JacobianPoint(x3, y3, z3)

def jacobian_point_double(p):
    """Jacobian坐标下的点倍乘"""
    if p.z == 0:
        return p
    
    y_sq = mod_mul(p.y, p.y)
    s = mod_mul(4, mod_mul(p.x, y_sq))
    m = mod_mul(3, mod_mul(p.x, p.x))
    
    if A != 0:
        m = mod_add(m, mod_mul(mod_mul(p.z, mod_mul(p.z, mod_mul(p.z, p.z))), A))
    
    x = mod_sub(mod_mul(m, m), mod_mul(2, s))
    y = mod_sub(mod_mul(m, mod_sub(s, x)), mod_mul(8, mod_mul(y_sq, y_sq)))
    z = mod_mul(2, mod_mul(p.y, p.z))
    
    return JacobianPoint(x, y, z)

def jacobian_point_mul(k, point):
    """Jacobian坐标下的点乘算法"""
    result = JacobianPoint(0, 0, 0)  # 无穷远点
    addend = to_jacobian(point)
    
    while k:
        if k & 1:
            result = jacobian_point_add(result, addend)
        addend = jacobian_point_double(addend)
        k >>= 1
    
    return from_jacobian(result)

# 并行点乘优化
def parallel_point_mul(k, point, num_processes=4):
    """并行点乘算法"""
    # 将k分解为多个部分
    k_bits = k.bit_length()
    chunk_size = (k_bits + num_processes - 1) // num_processes
    chunks = []
    
    for i in range(num_processes):
        start = i * chunk_size
        end = min((i + 1) * chunk_size, k_bits)
        chunk_mask = (1 << (end - start)) - 1
        chunks.append((k >> start) & chunk_mask)
    
    # 并行计算每个部分
    with multiprocessing.Pool(processes=num_processes) as pool:
        results = pool.starmap(
            point_mul_window, 
            [(chunk, point) for chunk in chunks]
        )
    
    # 合并结果
    result = INFINITY
    for i, res in enumerate(results):
        shift = i * chunk_size
        shifted_res = point_mul_window(1 << shift, res)
        result = point_add(result, shifted_res)
    
    return result

# SM2 密钥生成
def generate_keypair():
    """生成SM2密钥对"""
    d = random.randint(1, N - 1)
    public_key = point_mul_window(d, G)
    return d, public_key

# SM2 加密
def sm2_encrypt(public_key, plaintext):
    """SM2加密算法"""
    # 生成临时密钥对
    k = random.randint(1, N - 1)
    c1 = point_mul_window(k, G)
    
    # 计算共享密钥
    s = point_mul_window(k, public_key)
    if s.is_infinity:
        raise ValueError("Shared key is infinity")
    
    # 派生密钥
    x_bytes = s.x.to_bytes(32, 'big')
    y_bytes = s.y.to_bytes(32, 'big')
    key = hashlib.sha256(x_bytes + y_bytes).digest()
    
    # 加密数据
    ciphertext = bytes([p ^ k for p, k in zip(plaintext, key)])
    
    # 计算哈希验证
    hash_data = x_bytes + plaintext + y_bytes
    c3 = hashlib.sha256(hash_data).digest()
    
    # 返回密文 (C1 || C3 || C2)
    c1_bytes = b'\x04' + c1.x.to_bytes(32, 'big') + c1.y.to_bytes(32, 'big')
    return c1_bytes + c3 + ciphertext

# SM2 解密
def sm2_decrypt(private_key, ciphertext):
    """SM2解密算法"""
    # 解析密文
    if ciphertext[0] != 0x04:
        raise ValueError("Invalid ciphertext format")
    
    c1 = ECPoint(
        int.from_bytes(ciphertext[1:33], 'big'),
        int.from_bytes(ciphertext[33:65], 'big')
    )
    c3 = ciphertext[65:97]
    c2 = ciphertext[97:]
    
    # 计算共享密钥
    s = point_mul_window(private_key, c1)
    if s.is_infinity:
        raise ValueError("Shared key is infinity")
    
    # 派生密钥
    x_bytes = s.x.to_bytes(32, 'big')
    y_bytes = s.y.to_bytes(32, 'big')
    key = hashlib.sha256(x_bytes + y_bytes).digest()
    
    # 解密数据
    plaintext = bytes([c ^ k for c, k in zip(c2, key)])
    
    # 验证哈希
    hash_data = x_bytes + plaintext + y_bytes
    expected_c3 = hashlib.sha256(hash_data).digest()
    
    if c3 != expected_c3:
        raise ValueError("Hash verification failed")
    
    return plaintext

# SM2 签名
def sm2_sign(private_key, message, user_id=b"1234567812345678"):
    """SM2签名算法"""
    # 计算 ZA = Hash(ENTL || ID || a || b || Gx || Gy || Px || Py)
    entl = len(user_id).to_bytes(2, 'big')
    public_key = point_mul_window(private_key, G)
    
    za_data = entl + user_id
    za_data += A.to_bytes(32, 'big')
    za_data += B.to_bytes(32, 'big')
    za_data += Gx.to_bytes(32, 'big')
    za_data += Gy.to_bytes(32, 'big')
    za_data += public_key.x.to_bytes(32, 'big')
    za_data += public_key.y.to_bytes(32, 'big')
    
    za = hashlib.sha256(za_data).digest()
    
    # 计算 e = Hash(ZA || message)
    e_data = za + message
    e = hashlib.sha256(e_data).digest()
    e_int = int.from_bytes(e, 'big')
    
    # 签名循环
    while True:
        k = random.randint(1, N - 1)
        p = point_mul_window(k, G)
        r = (e_int + p.x) % N
        
        if r == 0 or r + k == N:
            continue
        
        s = (mod_inv(1 + private_key, N) * (k - r * private_key)) % N
        if s == 0:
            continue
        
        break
    
    return r.to_bytes(32, 'big') + s.to_bytes(32, 'big')

# SM2 验证
def sm2_verify(public_key, message, signature, user_id=b"1234567812345678"):
    """SM2验证算法"""
    # 解析签名
    r = int.from_bytes(signature[:32], 'big')
    s = int.from_bytes(signature[32:], 'big')
    
    if not (1 <= r < N) or not (1 <= s < N):
        return False
    
    # 计算 ZA
    entl = len(user_id).to_bytes(2, 'big')
    za_data = entl + user_id
    za_data += A.to_bytes(32, 'big')
    za_data += B.to_bytes(32, 'big')
    za_data += Gx.to_bytes(32, 'big')
    za_data += Gy.to_bytes(32, 'big')
    za_data += public_key.x.to_bytes(32, 'big')
    za_data += public_key.y.to_bytes(32, 'big')
    
    za = hashlib.sha256(za_data).digest()
    
    # 计算 e
    e_data = za + message
    e = hashlib.sha256(e_data).digest()
    e_int = int.from_bytes(e, 'big')
    
    # 计算 t
    t = (r + s) % N
    if t == 0:
        return False
    
    # 计算点
    p1 = point_mul_window(s, G)
    p2 = point_mul_window(t, public_key)
    point = point_add(p1, p2)
    
    # 验证 R
    r_prime = (e_int + point.x) % N
    return r_prime == r

# 性能测试
def performance_test():
    """性能测试函数"""
    # 生成测试数据
    private_key, public_key = generate_keypair()
    message = b"Test message for SM2 performance testing" * 10  # 400 bytes
    
    # 定义算法列表
    algorithms = [
        ("基础点乘", point_mul),
        ("滑动窗口", point_mul_window),
        ("Jacobian坐标", jacobian_point_mul),
        ("并行点乘", lambda k, p: parallel_point_mul(k, p, 4))
    ]
    
    # 测试密钥生成
    print("=== 密钥生成性能 ===")
    start = time.time()
    for _ in range(10):
        generate_keypair()
    end = time.time()
    print(f"密钥生成平均时间: {(end - start)/10:.6f} 秒")
    print()
    
    # 测试点乘算法
    print("=== 点乘算法性能 (100次点乘) ===")
    for name, func in algorithms:
        start = time.time()
        for _ in range(100):
            func(random.randint(1, N-1), G)
        end = time.time()
        print(f"{name}: {(end - start):.4f} 秒")
    print()
    
    # 测试加密性能
    print("=== 加密性能 (100次加密) ===")
    start = time.time()
    for _ in range(100):
        sm2_encrypt(public_key, message)
    end = time.time()
    print(f"加密平均时间: {(end - start)/100:.6f} 秒")
    print()
    
    # 测试解密性能
    ciphertext = sm2_encrypt(public_key, message)
    print("=== 解密性能 (100次解密) ===")
    start = time.time()
    for _ in range(100):
        sm2_decrypt(private_key, ciphertext)
    end = time.time()
    print(f"解密平均时间: {(end - start)/100:.6f} 秒")
    print()
    
    # 测试签名性能
    print("=== 签名性能 (100次签名) ===")
    start = time.time()
    for _ in range(100):
        sm2_sign(private_key, message)
    end = time.time()
    print(f"签名平均时间: {(end - start)/100:.6f} 秒")
    print()
    
    # 测试验证性能
    signature = sm2_sign(private_key, message)
    print("=== 验证性能 (100次验证) ===")
    start = time.time()
    for _ in range(100):
        sm2_verify(public_key, message, signature)
    end = time.time()
    print(f"验证平均时间: {(end - start)/100:.6f} 秒")

# 功能测试
def functional_test():
    """功能测试函数"""
    print("=== SM2 功能测试 ===")
    
    # 生成密钥对
    private_key, public_key = generate_keypair()
    print(f"私钥: 0x{private_key:064x}")
    print(f"公钥: {public_key}")
    
    # 测试加密解密
    message = b"Hello, SM2 encryption!"
    print(f"\n原始消息: {message}")
    
    ciphertext = sm2_encrypt(public_key, message)
    print(f"密文长度: {len(ciphertext)} 字节")
    
    decrypted = sm2_decrypt(private_key, ciphertext)
    print(f"解密消息: {decrypted}")
    
    assert message == decrypted, "加密解密失败"
    print("加密解密测试通过!")
    
    # 测试签名验证
    message_to_sign = b"Important message for signing"
    print(f"\n签名消息: {message_to_sign}")
    
    signature = sm2_sign(private_key, message_to_sign)
    print(f"签名长度: {len(signature)} 字节")
    
    is_valid = sm2_verify(public_key, message_to_sign, signature)
    print(f"签名验证结果: {is_valid}")
    
    assert is_valid, "签名验证失败"
    print("签名验证测试通过!")
    
    # 测试无效签名
    invalid_signature = bytes([b ^ 0xFF for b in signature])
    is_valid = sm2_verify(public_key, message_to_sign, invalid_signature)
    print(f"无效签名验证结果: {is_valid}")
    
    assert not is_valid, "无效签名验证错误"
    print("无效签名测试通过!")
    
    print("\n所有功能测试通过!")

if __name__ == "__main__":
    functional_test()
    print("\n\n")
    performance_test()