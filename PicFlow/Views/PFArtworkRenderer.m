//
//  PFArtworkRenderer.m
//  PicFlow
//

#import "PFArtworkRenderer.h"
#import "../Utils/PFLog.h"
#include "../../core/picflow_palette.h"
#include "../../core/picflow_catalog.h"

@implementation PFArtworkRenderer

+ (UIImage *)renderArtworkWithSeed:(uint32_t)seed size:(CGSize)size scale:(CGFloat)scale {
    if (size.width <= 0 || size.height <= 0) {
        return nil;
    }
    if (scale <= 0) {
        scale = 1.0;
    }

    // 1. 由 C 配色引擎推导整张图的配色与图案规格。
    PFArtworkSpec spec;
    pf_palette_generate((unsigned int)seed, &spec);

    // 2. 用 UIGraphicsImageRenderer 绘制（iOS 10+，线程安全）。
    UIGraphicsImageRendererFormat *format = [UIGraphicsImageRendererFormat defaultFormat];
    format.scale = scale;
    format.opaque = YES;

    UIGraphicsImageRenderer *renderer =
        [[UIGraphicsImageRenderer alloc] initWithSize:size format:format];

    UIImage *image = [renderer imageWithActions:^(UIGraphicsImageRendererContext *context) {
        CGContextRef ctx = context.CGContext;
        CGRect bounds = CGRectMake(0, 0, size.width, size.height);

        // 3. 背景填充
        UIColor *bgColor = [PFArtworkRenderer colorFromPFColor:spec.background];
        [bgColor setFill];
        CGContextFillRect(ctx, bounds);

        // 4. 按图案类型绘制
        UIColor *primary = [PFArtworkRenderer colorFromPFColor:spec.primary];
        UIColor *secondary = [PFArtworkRenderer colorFromPFColor:spec.secondary];
        UIColor *accent = [PFArtworkRenderer colorFromPFColor:spec.accent];
        UIColor *lineColor = [PFArtworkRenderer colorFromPFColor:spec.line];

        switch (spec.pattern) {
            case PF_PATTERN_CIRCLES:
                [self drawCirclesInContext:ctx
                                    bounds:bounds
                                     color:primary
                                     count:spec.motif_count
                                   density:spec.density];
                break;
            case PF_PATTERN_DIAGONAL:
                [self drawDiagonalInContext:ctx
                                    bounds:bounds
                                     color:primary
                                     count:spec.motif_count
                                   density:spec.density];
                break;
            case PF_PATTERN_WAVES:
                [self drawWavesInContext:ctx
                                 bounds:bounds
                                  color:primary
                                  count:spec.motif_count
                                density:spec.density];
                break;
            case PF_PATTERN_TRIANGLES:
                [self drawTrianglesInContext:ctx
                                     bounds:bounds
                                      color:accent
                                      count:spec.motif_count
                                    density:spec.density];
                break;
            case PF_PATTERN_GRID:
            default:
                [self drawGridInContext:ctx
                               bounds:bounds
                         primaryColor:primary
                      secondaryColor:secondary
                                count:spec.motif_count
                              density:spec.density];
                break;
        }

        // 5. 描边
        CGContextSetStrokeColorWithColor(ctx, lineColor.CGColor);
        CGContextSetLineWidth(ctx, 1.0);
        CGContextStrokeRect(ctx, CGRectInset(bounds, 0.5, 0.5));
    }];

    return image;
}

+ (UIImage *)renderArtworkForCatalogIndex:(NSInteger)index
                                    width:(CGFloat)width
                                     scale:(CGFloat)scale {
    PFThumbnailRequest request;
    pf_catalog_thumbnail_request((int)index, (int)(width * scale), &request);
    CGSize size = CGSizeMake(request.pixel_width / MAX(scale, 1.0),
                              request.pixel_height / MAX(scale, 1.0));
    return [self renderArtworkWithSeed:request.seed size:size scale:scale];
}

#pragma mark - 图案绘制

+ (void)drawCirclesInContext:(CGContextRef)ctx
                      bounds:(CGRect)bounds
                       color:(UIColor *)color
                       count:(NSInteger)count
                     density:(double)density {
    CGContextSetStrokeColorWithColor(ctx, color.CGColor);
    CGContextSetLineWidth(ctx, 1.6);
    CGContextSetAlpha(ctx, 0.75);

    double radius = MIN(bounds.size.width, bounds.size.height) / (double)(count / 2 + 2);
    if (radius < 6) {
        radius = 6;
    }
    for (NSInteger i = 0; i < count; i++) {
        double cx = CGRectGetMidX(bounds) + ((i % 3) - 1) * radius * 0.55;
        double cy = CGRectGetMinY(bounds) + bounds.size.height * (0.22 + 0.18 * (i % 4));
        if (cy - radius > CGRectGetMaxY(bounds)) {
            break;
        }
        CGContextAddEllipseInRect(ctx, CGRectMake(cx - radius, cy - radius, radius * 2, radius * 2));
        CGContextStrokePath(ctx);
    }
}

+ (void)drawDiagonalInContext:(CGContextRef)ctx
                        bounds:(CGRect)bounds
                         color:(UIColor *)color
                         count:(NSInteger)count
                       density:(double)density {
    CGContextSetStrokeColorWithColor(ctx, color.CGColor);
    CGContextSetLineWidth(ctx, 2.0);
    CGContextSetAlpha(ctx, 0.7);

    double step = CGRectGetWidth(bounds) / (double)(count + 1);
    for (NSInteger i = 0; i < count; i++) {
        double x = CGRectGetMinX(bounds) + step * (i + 1);
        CGContextMoveToPoint(ctx, x, CGRectGetMaxY(bounds));
        CGContextAddLineToPoint(ctx, x + CGRectGetHeight(bounds) * 0.45, CGRectGetMinY(bounds));
        CGContextStrokePath(ctx);
    }
}

+ (void)drawWavesInContext:(CGContextRef)ctx
                     bounds:(CGRect)bounds
                      color:(UIColor *)color
                      count:(NSInteger)count
                    density:(double)density {
    CGContextSetStrokeColorWithColor(ctx, color.CGColor);
    CGContextSetLineWidth(ctx, 2.0);
    CGContextSetAlpha(ctx, 0.72);

    NSInteger waves = count / 2 + 2;
    double waveHeight = CGRectGetHeight(bounds) / (double)(waves + 1);
    for (NSInteger i = 0; i < waves; i++) {
        double y = CGRectGetMinY(bounds) + waveHeight * (i + 1);
        double controlY = y - waveHeight * 0.35;
        CGContextMoveToPoint(ctx, CGRectGetMinX(bounds), y);
        CGContextAddQuadCurveToPoint(ctx, CGRectGetMidX(bounds), controlY,
                                     CGRectGetMaxX(bounds), y);
        CGContextStrokePath(ctx);
    }
}

+ (void)drawTrianglesInContext:(CGContextRef)ctx
                         bounds:(CGRect)bounds
                          color:(UIColor *)color
                          count:(NSInteger)count
                        density:(double)density {
    CGContextSetFillColorWithColor(ctx, color.CGColor);
    CGContextSetAlpha(ctx, 0.45);

    double side = CGRectGetWidth(bounds) * 0.16;
    double step = CGRectGetHeight(bounds) / (double)(count + 1);
    for (NSInteger i = 0; i < count; i++) {
        double y = CGRectGetMinY(bounds) + step * (i + 1) - side * 0.5;
        if (y + side > CGRectGetMaxY(bounds)) {
            break;
        }
        double cx = CGRectGetMidX(bounds);
        CGContextMoveToPoint(ctx, cx, y);
        CGContextAddLineToPoint(ctx, cx + side, y + side);
        CGContextAddLineToPoint(ctx, cx - side, y + side);
        CGContextClosePath(ctx);
        CGContextFillPath(ctx);
    }
}

+ (void)drawGridInContext:(CGContextRef)ctx
                    bounds:(CGRect)bounds
              primaryColor:(UIColor *)primary
           secondaryColor:(UIColor *)secondary
                     count:(NSInteger)count
                   density:(double)density {
    NSInteger columns = 3;
    NSInteger rows = MAX(2, (NSInteger)(count / columns) + 1);
    double cellW = CGRectGetWidth(bounds) / columns;
    double cellH = CGRectGetHeight(bounds) / rows;

    for (NSInteger row = 0; row < rows; row++) {
        for (NSInteger col = 0; col < columns; col++) {
            NSInteger index = row * columns + col;
            if (index >= count) {
                break;
            }
            CGRect cell = CGRectMake(CGRectGetMinX(bounds) + col * cellW + cellW * 0.12,
                                     CGRectGetMinY(bounds) + row * cellH + cellH * 0.12,
                                     cellW * 0.76, cellH * 0.76);
            UIColor *fill = (index % 2 == 0) ? primary : secondary;
            CGContextSetFillColorWithColor(ctx, fill.CGColor);
            CGContextSetAlpha(ctx, 0.45);
            CGContextFillEllipseInRect(ctx, cell);
        }
    }
}

#pragma mark - 颜色转换

+ (UIColor *)colorFromPFColor:(PFColor)color {
    return [UIColor colorWithRed:(CGFloat)(color.r / 255.0)
                           green:(CGFloat)(color.g / 255.0)
                            blue:(CGFloat)(color.b / 255.0)
                           alpha:1.0];
}

@end
