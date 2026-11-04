//
//  PFLog.h
//  PicFlow
//
//  统一日志封装。
//  使用 os_log 风格的统一前缀，方便在控制台用 `log stream --predicate` 过滤。
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, PFLogLevel) {
    PFLogLevelDebug = 0,
    PFLogLevelInfo,
    PFLogLevelWarning,
    PFLogLevelError,
};

FOUNDATION_EXPORT PFLogLevel PFLogCurrentLevel(void);
FOUNDATION_EXPORT void PFLogSetLevel(PFLogLevel level);

FOUNDATION_EXPORT void PFLogWrite(PFLogLevel level,
                                  const char *file,
                                  int line,
                                  NSString *format, ...) NS_FORMAT_FUNCTION(4, 5);

#define PFLogDebug(fmt, ...)  PFLogWrite(PFLogLevelDebug,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PFLogInfo(fmt, ...)   PFLogWrite(PFLogLevelInfo,   __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PFLogWarning(fmt, ...) PFLogWrite(PFLogLevelWarning, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define PFLogError(fmt, ...)  PFLogWrite(PFLogLevelError,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)

NS_ASSUME_NONNULL_END
