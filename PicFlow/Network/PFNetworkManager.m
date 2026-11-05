//
//  PFNetworkManager.m
//  PicFlow
//

#import "PFNetworkManager.h"
#import "../Utils/PFLog.h"

NSString * const PFNetworkErrorDomain = @"com.picflow.network";

@implementation PFNetworkError

+ (NSError *)errorWithCode:(PFNetworkErrorCode)code
               description:(NSString *)description
           underlyingError:(NSError *)underlying {
    NSMutableDictionary *userInfo = [NSMutableDictionary dictionary];
    userInfo[NSLocalizedDescriptionKey] = description ?: @"网络请求失败";
    if (underlying) {
        userInfo[NSUnderlyingErrorKey] = underlying;
    }
    return [NSError errorWithDomain:PFNetworkErrorDomain code:code userInfo:userInfo];
}

+ (NSError *)errorFromUnderlying:(NSError *)error {
    PFNetworkErrorCode code = PFNetworkErrorCodeUnknown;
    NSString *desc = error.localizedDescription ?: @"网络请求失败";

    if ([error.domain isEqualToString:NSURLErrorDomain]) {
        switch (error.code) {
            case NSURLErrorTimedOut:
                code = PFNetworkErrorCodeTimeout;
                desc = @"请求超时，请检查网络后重试";
                break;
            case NSURLErrorNotConnectedToInternet:
            case NSURLErrorNetworkConnectionLost:
                code = PFNetworkErrorCodeNoConnection;
                desc = @"网络已断开，请检查网络连接";
                break;
            case NSURLErrorCancelled:
                code = PFNetworkErrorCodeCancelled;
                desc = @"请求已取消";
                break;
            default:
                code = PFNetworkErrorCodeUnknown;
                break;
        }
    }
    return [self errorWithCode:code description:desc underlyingError:error];
}

@end

@interface PFNetworkManager ()
@property (nonatomic, strong) NSURLSession *session;
@end

@implementation PFNetworkManager

+ (instancetype)sharedManager {
    static PFNetworkManager *manager = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        manager = [[PFNetworkManager alloc] init];
    });
    return manager;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        NSURLSessionConfiguration *configuration = [NSURLSessionConfiguration defaultSessionConfiguration];
        configuration.timeoutIntervalForRequest = 15.0;
        configuration.timeoutIntervalForResource = 30.0;
        configuration.HTTPAdditionalHeaders = @{ @"Accept": @"application/json" };
        // 使用系统 URL 缓存，避免重复请求同一张图。
        configuration.requestCachePolicy = NSURLRequestUseProtocolCachePolicy;
        _session = [NSURLSession sessionWithConfiguration:configuration];
    }
    return self;
}

#pragma mark - GET

- (void)GET:(NSString *)urlString
  parameters:(NSDictionary<NSString *, id> *)parameters
     success:(PFNetworkSuccessBlock)success
     failure:(PFNetworkFailureBlock)failure {
    [self GET:urlString parameters:parameters success:success failure:failure cancelToken:nil];
}

- (NSURLSessionDataTask *)GET:(NSString *)urlString
                    parameters:(NSDictionary<NSString *, id> *)parameters
                       success:(PFNetworkSuccessBlock)success
                       failure:(PFNetworkFailureBlock)failure
                   cancelToken:(void (^)(void (^cancelAction)(void)))cancelToken {
    NSURL *url = [self buildURLWithString:urlString parameters:parameters];
    if (!url) {
        PFLogError(@"URL 构造失败: %@", urlString);
        if (failure) {
            failure([PFNetworkError errorWithCode:PFNetworkErrorCodeBadURL
                                      description:@"请求地址不合法"
                                  underlyingError:nil]);
        }
        return nil;
    }

    PFLogDebug(@"→ GET %@", url.absoluteString);

    NSURLSessionDataTask *task =
    [self.session dataTaskWithURL:url
                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        // 网络层回调不在主线程，统一切回主线程给上层用（简化上层心智负担）。
        dispatch_async(dispatch_get_main_queue(), ^{
            if (error) {
                NSError *translated = [PFNetworkError errorFromUnderlying:error];
                PFLogError(@"← FAIL %@ | %@", url.path, translated.localizedDescription);
                if (failure) {
                    failure(translated);
                }
                return;
            }

            NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
            if (http.statusCode < 200 || http.statusCode >= 300) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeBadResponse
                                                 description:[NSString stringWithFormat:@"服务器返回异常状态码 %ld", (long)http.statusCode]
                                             underlyingError:nil];
                PFLogError(@"← BAD STATUS %ld | %@", (long)http.statusCode, url.path);
                if (failure) {
                    failure(err);
                }
                return;
            }

            if (data.length == 0) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeEmptyData
                                                 description:@"服务器返回空数据"
                                             underlyingError:nil];
                if (failure) {
                    failure(err);
                }
                return;
            }

            NSError *jsonError = nil;
            id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:&jsonError];
            if (!json) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeParseFailed
                                                 description:@"数据解析失败"
                                             underlyingError:jsonError];
                PFLogError(@"← PARSE FAIL %@ | %@", url.path, jsonError.localizedDescription);
                if (failure) {
                    failure(err);
                }
                return;
            }

            PFLogDebug(@"← OK %@ | %lu bytes", url.path, (unsigned long)data.length);
            if (success) {
                success(json);
            }
        });
    }];

    if (cancelToken) {
        cancelToken(^{ [task cancel]; });
    }

    [task resume];
    return task;
}

#pragma mark - 二进制下载

- (NSURLSessionDataTask *)GETData:(NSString *)urlString
                           success:(void (^)(NSData *data))success
                           failure:(PFNetworkFailureBlock)failure {
    NSURL *url = [self buildURLWithString:urlString parameters:nil];
    if (!url) {
        if (failure) {
            failure([PFNetworkError errorWithCode:PFNetworkErrorCodeBadURL
                                      description:@"请求地址不合法"
                                  underlyingError:nil]);
        }
        return nil;
    }

    PFLogDebug(@"→ GET(binary) %@", url.absoluteString);

    NSURLSessionDataTask *task =
    [self.session dataTaskWithURL:url
                completionHandler:^(NSData *data, NSURLResponse *response, NSError *error) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (error) {
                NSError *translated = [PFNetworkError errorFromUnderlying:error];
                PFLogError(@"← FAIL(binary) %@ | %@", url.path, translated.localizedDescription);
                if (failure) { failure(translated); }
                return;
            }
            NSHTTPURLResponse *http = (NSHTTPURLResponse *)response;
            if (http.statusCode < 200 || http.statusCode >= 300) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeBadResponse
                                                 description:[NSString stringWithFormat:@"服务器返回异常状态码 %ld", (long)http.statusCode]
                                             underlyingError:nil];
                if (failure) { failure(err); }
                return;
            }
            if (data.length == 0) {
                NSError *err = [PFNetworkError errorWithCode:PFNetworkErrorCodeEmptyData
                                                 description:@"服务器返回空数据"
                                             underlyingError:nil];
                if (failure) { failure(err); }
                return;
            }
            PFLogDebug(@"← OK(binary) %@ | %lu bytes", url.path, (unsigned long)data.length);
            if (success) { success(data); }
        });
    }];
    [task resume];
    return task;
}

#pragma mark - URL

- (nullable NSURL *)buildURLWithString:(NSString *)urlString
                             parameters:(nullable NSDictionary<NSString *, id> *)parameters {
    NSURLComponents *components = [NSURLComponents componentsWithString:urlString];
    if (!components) {
        return nil;
    }
    if (parameters.count > 0) {
        NSMutableArray<NSURLQueryItem *> *items = [NSMutableArray array];
        [parameters enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
            [items addObject:[NSURLQueryItem queryItemWithName:key value:[value description]]];
        }];
        components.queryItems = items;
    }
    return components.URL;
}

@end
