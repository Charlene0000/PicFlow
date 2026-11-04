//
//  PFArtworkRenderer.h
//  PicFlow
//
//  程序化图片生成器 —— 本项目的图片**不来自任何外部 API 或素材库**，
//  而是由 Core Graphics 按 C 配色引擎给出的规格实时绘制。
//
//  这样做的好处：
//  1. 零外部依赖：不需要 API Key、不受网络与配额限制、离线可跑、CI 可复现；
//  2. 版权干净：没有任何第三方素材的授权风险，可放心商用、可给面试官看；
//  3. 确定性：同一个 seed 永远画出同一张图，缓存 key 与内容严格对应；
//  4. 更轻：矢量绘制按目标尺寸实时渲染，比加载一张大图再缩放省内存得多。
//
//  生成的图案类型与配色规则由 core/picflow_palette.c 决定，
//  iOS 端只负责把数值翻译成 Core Graphics 的绘制指令。
//

#import <UIKit/UIKit.h>

NS_ASSUME_NONNULL_BEGIN

@interface PFArtworkRenderer : NSObject

/// 依据规格生成图片。**必须在后台线程调用**（Core Graphics 绘制有 CPU 开销）。
/// @param seed      随机种子，决定配色与图案
/// @param size      目标像素尺寸
/// @param scale     屏幕缩放因子
/// @return 生成的 UIImage，失败返回 nil
+ (nullable UIImage *)renderArtworkWithSeed:(uint32_t)seed
                                       size:(CGSize)size
                                      scale:(CGFloat)scale;

/// 便捷方法：按图库索引生成。
+ (nullable UIImage *)renderArtworkForCatalogIndex:(NSInteger)index
                                          width:(CGFloat)width
                                           scale:(CGFloat)scale;

@end

NS_ASSUME_NONNULL_END
