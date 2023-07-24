#import <Cocoa/Cocoa.h>

void clipboard_write(const char *text) {
  @autoreleasepool {
    NSString *stringToCopy = [NSString stringWithUTF8String:text];
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    [pasteboard clearContents];
    [pasteboard writeObjects:@[stringToCopy]];
  }
}

char *clipboard_read() {
  @autoreleasepool {
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSArray *classes = @[[NSString class]];
    NSDictionary *options = @{};
    NSArray *copiedItems = [pasteboard readObjectsForClasses:classes options:options];

    if (copiedItems && [copiedItems count] > 0) {
      NSString *copiedString = copiedItems[0];
      const char *cString = [copiedString UTF8String];
      return strdup(cString);
    }
  }
  return NULL;
}

