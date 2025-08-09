import cv2
import numpy as np
import pywt
from skimage.util import random_noise

def embed_watermark(image_path, watermark_path, output_path, alpha=0.1):
    # 读取原始图像和水印图像
    original_image = cv2.imread(image_path, cv2.IMREAD_GRAYSCALE)
    watermark_image = cv2.imread(watermark_path, cv2.IMREAD_GRAYSCALE)

    # 确保水印图像是二值化的
    _, watermark_image = cv2.threshold(watermark_image, 127, 255, cv2.THRESH_BINARY)

    # 将水印图像调整到适合嵌入的大小
    watermark_resized = cv2.resize(watermark_image, (original_image.shape[1] // 4, original_image.shape[0] // 4))

    # 进行DWT分解
    coeffs = pywt.dwt2(original_image, 'haar')
    LL, (LH, HL, HH) = coeffs

    # 嵌入水印到HH子带中
    watermark_shape = watermark_resized.shape
    LL_mod = LL.copy()
    LL_mod[:watermark_shape[0], :watermark_shape[1]] += alpha * watermark_resized

    # 组合所有子带并重构图像
    coeffs_mod = LL_mod, (LH, HL, HH)
    watermarked_image = pywt.idwt2(coeffs_mod, 'haar')

    # 保存嵌入水印后的图像
    cv2.imwrite(output_path, watermarked_image.astype(np.uint8))
    return watermarked_image

def extract_watermark(embedded_image_path, original_watermark_path, alpha=0.1):
    # 读取嵌入水印后的图像和原始水印图像
    embedded_image = cv2.imread(embedded_image_path, cv2.IMREAD_GRAYSCALE)
    original_watermark = cv2.imread(original_watermark_path, cv2.IMREAD_GRAYSCALE)

    # 确保水印图像是二值化的
    _, original_watermark = cv2.threshold(original_watermark, 127, 255, cv2.THRESH_BINARY)

    # 将水印图像调整到适合嵌入的大小
    watermark_shape = (embedded_image.shape[1] // 4, embedded_image.shape[0] // 4)
    original_watermark_resized = cv2.resize(original_watermark, watermark_shape)

    # 对嵌入水印后的图像进行DWT分解
    coeffs_embedded = pywt.dwt2(embedded_image, 'haar')
    LL_embedded, (_, _, _) = coeffs_embedded

    # 提取嵌入的水印
    extracted_watermark = LL_embedded[:watermark_shape[0], :watermark_shape[1]]
    extracted_watermark -= LL_embedded[watermark_shape[0]:2*watermark_shape[0], :watermark_shape[1]].mean()

    # 归一化提取出的水印
    extracted_watermark = np.clip(extracted_watermark / alpha, 0, 255).astype(np.uint8)

    # 返回提取出的水印图像
    return extracted_watermark

def robustness_test(embedded_image_path, original_watermark_path, transformations):
    embedded_image = cv2.imread(embedded_image_path, cv2.IMREAD_GRAYSCALE)
    results = {}

    for transform in transformations:
        if transform == 'flip':
            transformed_image = cv2.flip(embedded_image, 1)  # 水平翻转
        elif transform == 'translate':
            M = np.float32([[1, 0, 50], [0, 1, 50]])  # 平移矩阵
            transformed_image = cv2.warpAffine(embedded_image, M, (embedded_image.shape[1], embedded_image.shape[0]))
        elif transform == 'crop':
            transformed_image = embedded_image[50:-50, 50:-50]  # 截取
        elif transform == 'contrast':
            transformed_image = cv2.convertScaleAbs(embedded_image, alpha=1.5, beta=0)  # 调对比度
        else:
            continue

        extracted_watermark = extract_watermark(transformed_image, original_watermark_path)
        psnr = compare_psnr(original_watermark_path, extracted_watermark)
        ssim = compare_ssim(original_watermark_path, extracted_watermark)

        results[transform] = {'PSNR': psnr, 'SSIM': ssim}

    return results

def compare_psnr(original_path, image):
    original = cv2.imread(original_path, cv2.IMREAD_GRAYSCALE)
    mse = np.mean((original - image) ** 2)
    if mse == 0:
        return float('inf')
    max_pixel = 255.0
    psnr = 20 * np.log10(max_pixel / np.sqrt(mse))
    return psnr

def compare_ssim(original_path, image):
    from skimage.metrics import structural_similarity as ssim
    original = cv2.imread(original_path, cv2.IMREAD_GRAYSCALE)
    s = ssim(original, image)
    return s

if __name__ == "__main__":
    original_image_path = 'path_to_original_image.jpg'
    watermark_image_path = 'path_to_watermark_image.png'
    watermarked_image_path = 'output_watermarked_image.jpg'

    # 嵌入水印
    embed_watermark(original_image_path, watermark_image_path, watermarked_image_path)

    # 提取水印
    extracted_watermark = extract_watermark(watermarked_image_path, watermark_image_path)
    cv2.imwrite('extracted_watermark.png', extracted_watermark)

    # 鲁棒性测试
    transformations = ['flip', 'translate', 'crop', 'contrast']
    robustness_results = robustness_test(watermarked_image_path, watermark_image_path, transformations)
    print(robustness_results)



