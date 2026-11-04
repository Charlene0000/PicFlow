//
//  PFNetworkManager.h
//  PicFlow
//
//  Network 层：基于 NSURLSession 的极简网络封装。
//  特性：
//  1. 统一的错误对象（PFNetworkError），把底层 NSError 翻译成业务可读信息；
//  2. 统一日志前缀，方便在控制台过滤；
//  3. 泛型 JSON 解析，解析失败不崩溃；
//  4. 断点/超时/无网络全部走同一条失败回调。
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

extern NSString * const PFNetworkErrorDomain;

typedef NS_ENUM(NSInteger, PFNetworkErrorCode) {
    PFNetworkErrorCodeBadURL          = -1000,
    PFNetworkErrorCodeBadResponse     = -1001,
    PFNetworkErrorCodeEmptyData       = -1002,
    PFNetworkErrorCodeParseFailed     = -1003,
    PFNetworkErrorCodeTimeout         = -1004,
    PFNetworkErrorCodeNoConnection     = -1005,
    PFNetworkErrorCodeCancelled       = -1006,
    PFNetworkErrorCodeUnknown         = -1999,
};

@interface PFNetworkError : NSObject

+ (NSError *)errorWithCode:(PFNetworkErrorCode)code
               description:(NSString *)description
           underlyingError:(nullable NSError *)underlying;

+ (NSError *)errorFromUnderlying:(NSError *)error;

@end

typedef void (^PFNetworkSuccessBlock)(id _Nullable jsonObject);
typedef void (^PFNetworkFailureBlock)(NSError *error);

@interface PFNetworkManager : NSObject

+ (instancetype)sharedManager;

/// 发起 GET 请求。JSON 解析成功后回调 success，失败一律回调 failure。
/// @param urlString 完整 URL
/// @param parameters query 参数，nil 表示无参数
- (void)GET:(NSString *)urlString
  parameters:(nullable NSDictionary<NSString *, id> *)parameters
     success:(PFNetworkSuccessBlock)success
     failure:(PFNetworkFailureBlock)failure;

/// 发起 GET 请求并返回 NSURLSessionDataTask，调用方可主动 cancel。
/// Cell 复用时取消未完成的图片请求就靠它。
- (nullable NSURLSessionDataTask *)GET:(NSString *)urlString
                             parameters:(nullable NSDictionary<NSString *, id> *)parameters
                                success:(PFNetworkSuccessBlock)success
                                failure:(PFNetworkFailureBlock)failure
                            cancelToken:(void (^ _Nullable)(void))cancelToken;

@end

NS_ASSUME_NONNULL_END
