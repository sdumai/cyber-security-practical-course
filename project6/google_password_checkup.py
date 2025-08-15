import hashlib
import random
from sympy import isprime, primitive_root

class GooglePasswordCheckup:
    def __init__(self, bits=32):
        self.bits = bits
        self.p = self._generate_prime()
        self.g1 = primitive_root(self.p)
        self.g2 = self.g1  # 为简化，使用同一个生成元
        self.breached_creds = []  # 存储泄露凭证
        self.a_list = []  # Helper存储的a_i
        self.b_list = []  # Server存储的b_i

    def _generate_prime(self):
        """生成一个足够大的素数p"""
        while True:
            candidate = random.getrandbits(self.bits)
            if candidate > 2 and isprime(candidate):
                return candidate

    def H1(self, u, p):
        """将凭证映射到Z_p^*中的元素"""
        s = u + p
        hash_bytes = hashlib.sha256(s.encode()).digest()
        return 1 + int.from_bytes(hash_bytes, 'big') % (self.p - 1)

    def H2(self, u, p):
        """将凭证映射为定长字符串（用于最终比对）"""
        s = u + p
        return hashlib.sha256(s.encode()).hexdigest()[:8]  # 取8字符简化

    def H3(self, t, k):
        """将两个群元素映射为定长字符串"""
        s = f"{t},{k}"
        return hashlib.sha256(s.encode()).hexdigest()[:8]  # 与H2输出长度一致

    def add_breached_credential(self, u, p):
        """添加泄露凭证到数据库"""
        c_i = self.H1(u, p)
        a_i = random.randint(1, self.p - 2)
        b_i = c_i * pow(a_i, -1, self.p) % self.p  # a_i * b_i ≡ c_i (mod p)
        self.breached_creds.append((u, p))
        self.a_list.append(a_i)
        self.b_list.append(b_i)

    def client_step(self, u, p):
        """客户端协议步骤"""
        x = self.H1(u, p)
        y = self.H2(u, p)
        r = random.randint(1, self.p - 2)
        s = pow(self.g1, r, self.p)
        t = pow(self.g2, r, self.p)
        d = pow(x, r, self.p)
        return (s, t), (d, y)

    def helper_step(self, t):
        """Helper协议步骤"""
        t_i_list = [pow(t, a_i, self.p) for a_i in self.a_list]
        return t_i_list

    def server_step(self, d, y, t_i_list):
        """Server协议步骤"""
        for i, b_i in enumerate(self.b_list):
            k_i = pow(d, b_i, self.p)
            w_i = self.H3(t_i_list[i], k_i)
            if w_i == y:
                return True, i
        return False, -1

# 测试协议
def test_protocol():
    # 初始化协议（使用小素数加速，实际需大素数）
    gpc = GooglePasswordCheckup(bits=16)
    print(f"Generated prime p = {gpc.p}")
    print(f"Generator g1 = g2 = {gpc.g1}")

    # 添加泄露凭证
    breached_creds = [("user1", "password1"), ("user2", "password2")]
    for u, p in breached_creds:
        gpc.add_breached_credential(u, p)
    print(f"Added {len(breached_creds)} breached credentials")

    # 测试1: 使用泄露凭证
    test_cases = [
        ("user1", "password1", True),  # 泄露凭证
        ("user2", "password2", True),  # 泄露凭证
        ("safe_user", "safe_pass", False)  # 安全凭证
    ]

    for u, p, expected_breached in test_cases:
        (s, t), (d, y) = gpc.client_step(u, p)
        t_i_list = gpc.helper_step(t)
        is_breached, index = gpc.server_step(d, y, t_i_list)
        
        result = "PASSED" if is_breached == expected_breached else "FAILED"
        print(f"\nTest: ({u}, {p})")
        print(f"Expected breached: {expected_breached}, Actual: {is_breached} (Index: {index})")
        print(f"Result: {result}")

if __name__ == "__main__":
    test_protocol()