//
//  PFPhotoDetailViewController.h
//  PicFlow
//

#import <UIKit/UIKit.h>
#import "../Models/PFPhotoItem.h"

NS_ASSUME_NONNULL_BEGIN

@interface PFPhotoDetailViewController : UIViewController

- (instancetype)initWithItem:(PFPhotoItem *)item;

@end

NS_ASSUME_NONNULL_END
