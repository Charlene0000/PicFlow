//
//  PFLog.m
//  PicFlow
//

#import "PFLog.h"

static PFLogLevel gCurrentLevel = PFLogLevelInfo;

PFLogLevel PFLogCurrentLevel(void) {
    return gCurrentLevel;
}

void PFLogSetLevel(PFLogLevel level) {
    gCurrentLevel = level;
}

void PFLogWrite(PFLogLevel level, const char *file, int line, NSString *format, ...) {
    if (level < gCurrentLevel) {
        return;
    }
    va_list args;
    va_start(args, format);
    NSString *body = [[NSString alloc] initWithFormat:format arguments:args];
    va_end(args);

    const char *fileName = strrchr(file, '/');
    fileName = fileName ? fileName + 1 : file;

    static const char *tags[] = { "DEBUG", "INFO ", "WARN ", "ERROR" };
    NSString *tag = @(tags[level]);

    NSLog(@"[PicFlow][%@][%s:%d] %@", tag, fileName, line, body);
}
