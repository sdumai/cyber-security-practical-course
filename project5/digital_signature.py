import hashlib
import ecdsa
from ecdsa import SigningKey, SECP256k1, VerifyingKey
from ecdsa.util import sigencode_der, sigdecode_der

# 比特币使用的椭圆曲线参数
curve = SECP256k1
n = curve.order  # 曲线阶

satoshis_pubkey_hex = "04" + \
    "50863AD64A87AE8A2FE83C1AF1A8403CB53F53E486D8511DAD8A04887E5B2352" + \
    "2CD470243453A299FA9E77237716103ABC11A1DF38855ED6F2EE187E9C582BA6"
satoshis_pubkey = VerifyingKey.from_string(
    bytes.fromhex(satoshis_pubkey_hex[2:]), 
    curve=ecdsa.SECP256k1
)

def forge_signature(original_sig, original_msg):
    """
    伪造中本聪签名（利用ECDSA签名可延展性）
    
    参数:
        original_sig: 原始签名（DER格式）
        original_msg: 原始消息
        
    返回:
        forged_sig: 伪造的签名（DER格式）
        forged_msg: 伪造的消息
    """
    # 解码原始签名
    r, s = sigdecode_der(original_sig, n)
    
    # 计算新s值：s' = n - s
    s_forged = n - s
    
    # 编码伪造的签名
    forged_sig = sigencode_der(r, s_forged, n)
    
    # 创建伪造的消息（原始消息的变体）
    forged_msg = original_msg + b" (forged by attacker)"
    
    return forged_sig, forged_msg

def validate_signature(pubkey, msg, sig):
    """验证签名有效性"""
    try:
        return pubkey.verify(sig, msg, 
                            hashfunc=hashlib.sha256,
                            sigdecode=sigdecode_der)
    except:
        return False

if __name__ == "__main__":
    # 中本聪对原始消息签名
    original_msg = b"Transfer 1,000,000 BTC to Alice"

    private_key = SigningKey.generate(curve=SECP256k1)
    original_sig = private_key.sign(original_msg, 
                                   hashfunc=hashlib.sha256,
                                   sigencode=sigencode_der)
    
    # 验证原始签名有效
    print("原始签名验证:", validate_signature(satoshis_pubkey, original_msg, original_sig))
    
    # 攻击者伪造签名
    forged_sig, forged_msg = forge_signature(original_sig, original_msg)
    
    # 验证伪造签名
    is_valid_forgery = validate_signature(satoshis_pubkey, forged_msg, forged_sig)
    
    print("\n伪造签名验证结果:", is_valid_forgery)
    print("伪造的消息:", forged_msg.decode())
    print("原始签名:", original_sig.hex())
    print("伪造签名:", forged_sig.hex())
    
    # 攻击者可以广播伪造的签名到比特币网络
    if is_valid_forgery:
        print("\n攻击成功! 伪造签名通过中本聪公钥验证")
        print("比特币网络可能接受此伪造交易")