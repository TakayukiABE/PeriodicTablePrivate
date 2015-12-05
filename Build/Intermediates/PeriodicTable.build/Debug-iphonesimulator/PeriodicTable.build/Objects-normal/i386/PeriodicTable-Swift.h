// Generated by Apple Swift version 1.2 (swiftlang-602.0.49.6 clang-602.0.49)
#pragma clang diagnostic push

#if defined(__has_include) && __has_include(<swift/objc-prologue.h>)
# include <swift/objc-prologue.h>
#endif

#pragma clang diagnostic ignored "-Wauto-import"
#include <objc/NSObject.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#if defined(__has_include) && __has_include(<uchar.h>)
# include <uchar.h>
#elif !defined(__cplusplus) || __cplusplus < 201103L
typedef uint_least16_t char16_t;
typedef uint_least32_t char32_t;
#endif

typedef struct _NSZone NSZone;

#if !defined(SWIFT_PASTE)
# define SWIFT_PASTE_HELPER(x, y) x##y
# define SWIFT_PASTE(x, y) SWIFT_PASTE_HELPER(x, y)
#endif
#if !defined(SWIFT_METATYPE)
# define SWIFT_METATYPE(X) Class
#endif

#if defined(__has_attribute) && __has_attribute(objc_runtime_name)
# define SWIFT_RUNTIME_NAME(X) __attribute__((objc_runtime_name(X)))
#else
# define SWIFT_RUNTIME_NAME(X)
#endif
#if !defined(SWIFT_CLASS_EXTRA)
# define SWIFT_CLASS_EXTRA
#endif
#if !defined(SWIFT_PROTOCOL_EXTRA)
# define SWIFT_PROTOCOL_EXTRA
#endif
#if !defined(SWIFT_ENUM_EXTRA)
# define SWIFT_ENUM_EXTRA
#endif
#if !defined(SWIFT_CLASS)
# if defined(__has_attribute) && __has_attribute(objc_subclassing_restricted) 
#  define SWIFT_CLASS(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) __attribute__((objc_subclassing_restricted)) SWIFT_CLASS_EXTRA
# else
#  define SWIFT_CLASS(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) SWIFT_CLASS_EXTRA
# endif
#endif

#if !defined(SWIFT_PROTOCOL)
# define SWIFT_PROTOCOL(SWIFT_NAME) SWIFT_RUNTIME_NAME(SWIFT_NAME) SWIFT_PROTOCOL_EXTRA
#endif

#if !defined(SWIFT_EXTENSION)
# define SWIFT_EXTENSION(M) SWIFT_PASTE(M##_Swift_, __LINE__)
#endif

#if !defined(OBJC_DESIGNATED_INITIALIZER)
# if defined(__has_attribute) && __has_attribute(objc_designated_initializer)
#  define OBJC_DESIGNATED_INITIALIZER __attribute__((objc_designated_initializer))
# else
#  define OBJC_DESIGNATED_INITIALIZER
# endif
#endif
#if !defined(SWIFT_ENUM)
# define SWIFT_ENUM(_type, _name) enum _name : _type _name; enum SWIFT_ENUM_EXTRA _name : _type
#endif
#if __has_feature(nullability)
#  define SWIFT_NULLABILITY(X) X
#else
# if !defined(__nonnull)
#  define __nonnull
# endif
# if !defined(__nullable)
#  define __nullable
# endif
# if !defined(__null_unspecified)
#  define __null_unspecified
# endif
#  define SWIFT_NULLABILITY(X)
#endif
#if defined(__has_feature) && __has_feature(modules)
@import UIKit;
@import CoreGraphics;
@import RealmSwift;
@import ObjectiveC;
#endif

#pragma clang diagnostic ignored "-Wproperty-attribute-mismatch"
#pragma clang diagnostic ignored "-Wduplicate-method-arg"
@class UIWindow;
@class UIApplication;
@class NSObject;

SWIFT_CLASS("_TtC13PeriodicTable11AppDelegate")
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (nonatomic) UIWindow * __nullable window;
- (BOOL)application:(UIApplication * __nonnull)application didFinishLaunchingWithOptions:(NSDictionary * __nullable)launchOptions;
- (void)applicationWillResignActive:(UIApplication * __nonnull)application;
- (void)applicationDidEnterBackground:(UIApplication * __nonnull)application;
- (void)applicationWillEnterForeground:(UIApplication * __nonnull)application;
- (void)applicationDidBecomeActive:(UIApplication * __nonnull)application;
- (void)applicationWillTerminate:(UIApplication * __nonnull)application;
- (SWIFT_NULLABILITY(nonnull) instancetype)init OBJC_DESIGNATED_INITIALIZER;
@end

@class UILabel;
@class NSCoder;

SWIFT_CLASS("_TtC13PeriodicTable10DetailView")
@interface DetailView : UIView
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified symbol;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified name;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified massNumber;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified number;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified orbit;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified meltingPoint;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified boilingPoint;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified category;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified ionization;
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified affinity;
+ (DetailView * __nonnull)createInstance;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithFrame:(CGRect)frame OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end

@class NSBundle;

SWIFT_CLASS("_TtC13PeriodicTable20DetailViewController")
@interface DetailViewController : UIViewController
@property (nonatomic) DetailView * __nonnull detailView;
- (void)viewDidLoad;
- (void)viewDidAppear:(BOOL)animated;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithNibName:(NSString * __nullable)nibNameOrNil bundle:(NSBundle * __nullable)nibBundleOrNil OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end

@class RLMRealm;
@class RLMObjectSchema;
@class RLMSchema;

SWIFT_CLASS("_TtC13PeriodicTable7Element")
@interface Element : Object
@property (nonatomic, copy) NSString * __nonnull symbol;
@property (nonatomic, copy) NSString * __nonnull name;
@property (nonatomic) NSInteger number;
@property (nonatomic) double massNumber;
@property (nonatomic, copy) NSString * __nonnull state;
@property (nonatomic) NSInteger meltingPoint;
@property (nonatomic) NSInteger boilingPoint;
@property (nonatomic) double ionization;
@property (nonatomic) NSInteger affinity;
@property (nonatomic, copy) NSString * __nonnull orbit;
@property (nonatomic, copy) NSString * __nonnull category;
@property (nonatomic) NSInteger group;
@property (nonatomic) NSInteger period;
- (SWIFT_NULLABILITY(nonnull) instancetype)init OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithValue:(id __nonnull)value OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithRealm:(RLMRealm * __nonnull)realm schema:(RLMObjectSchema * __nonnull)schema OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithValue:(id __nonnull)value schema:(RLMSchema * __nonnull)schema OBJC_DESIGNATED_INITIALIZER;
@end


SWIFT_CLASS("_TtC13PeriodicTable26PeriodicCollectionViewCell")
@interface PeriodicCollectionViewCell : UICollectionViewCell
@property (nonatomic, weak) IBOutlet UILabel * __null_unspecified elementName;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithFrame:(CGRect)frame OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end


SWIFT_CLASS("_TtC13PeriodicTable13PeriodicModel")
@interface PeriodicModel : NSObject
@property (nonatomic, copy) NSArray * __nonnull elements;
@property (nonatomic) BOOL isLanthanide;
@property (nonatomic) BOOL isActinide;
- (SWIFT_NULLABILITY(nonnull) instancetype)init OBJC_DESIGNATED_INITIALIZER;
- (void)setElementProperties:(Element * __nonnull)element symbol:(NSString * __nonnull)symbol name:(NSString * __nonnull)name number:(NSInteger)number massNumber:(double)massNumber state:(NSString * __nonnull)state meltingPoint:(NSInteger)meltingPoint boilingPoint:(NSInteger)boilingPoint ionization:(double)ionization affinity:(NSInteger)affinity orbit:(NSString * __nonnull)orbit category:(NSString * __nonnull)category group:(NSInteger)group period:(NSInteger)period;
- (Element * __nonnull)readElement:(NSInteger)group period:(NSInteger)period cell:(NSArray * __nonnull)cell;
@end

@class UINib;
@class UICollectionView;
@class NSIndexPath;
@class UICollectionViewLayout;

SWIFT_CLASS("_TtC13PeriodicTable22PeriodicViewController")
@interface PeriodicViewController : UIViewController <UICollectionViewDelegate, UICollectionViewDataSource, UICollectionViewDelegateFlowLayout>
@property (nonatomic, readonly) UINib * __nonnull Nib;
@property (nonatomic) UICollectionView * __nonnull periodicTable;
@property (nonatomic) NSInteger temperature;
@property (nonatomic, copy) NSArray * __nonnull selectedCell;
@property (nonatomic) PeriodicModel * __nonnull periodicModel;
- (void)viewDidLoad;
- (void)viewWillLayoutSubviews;
- (NSInteger)numberOfSectionsInCollectionView:(UICollectionView * __nonnull)collectionView;
- (NSInteger)collectionView:(UICollectionView * __nonnull)collectionView numberOfItemsInSection:(NSInteger)section;
- (UICollectionViewCell * __nonnull)collectionView:(UICollectionView * __nonnull)collectionView cellForItemAtIndexPath:(NSIndexPath * __nonnull)indexPath;
- (CGSize)collectionView:(UICollectionView * __nonnull)collectionView layout:(UICollectionViewLayout * __nonnull)collectionViewLayout sizeForItemAtIndexPath:(NSIndexPath * __nonnull)indexPath;
- (CGFloat)collectionView:(UICollectionView * __nonnull)collectionView layout:(UICollectionViewLayout * __nonnull)collectionViewLayout minimumInteritemSpacingForSectionAtIndex:(NSInteger)section;
- (UIEdgeInsets)collectionView:(UICollectionView * __nonnull)collectionView layout:(UICollectionViewLayout * __nonnull)collectionViewLayout insetForSectionAtIndex:(NSInteger)section;
- (void)setCurrentTemperature:(NSInteger)newTemperature;
- (void)collectionView:(UICollectionView * __nonnull)collectionView didSelectItemAtIndexPath:(NSIndexPath * __nonnull)indexPath;
- (void)changeCell:(NSInteger)arrowTag;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithNibName:(NSString * __nullable)nibNameOrNil bundle:(NSBundle * __nullable)nibBundleOrNil OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end


SWIFT_CLASS("_TtC13PeriodicTable26SeparateCollectionViewCell")
@interface SeparateCollectionViewCell : UICollectionViewCell
- (void)awakeFromNib;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithFrame:(CGRect)frame OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end

@class UISlider;
@class UIButton;
@class UISwitch;

SWIFT_CLASS("_TtC13PeriodicTable14ViewController")
@interface ViewController : UIViewController
@property (nonatomic) UISlider * __nonnull temperatureSlider;
@property (nonatomic) UILabel * __nonnull temperature;
@property (nonatomic) NSInteger value;
@property (nonatomic) NSInteger newValue;
@property (nonatomic, copy) NSArray * __nonnull cursors;
@property (nonatomic) UISwitch * __nonnull handSwitch;
@property (nonatomic) PeriodicViewController * __nonnull periodicView;
@property (nonatomic) DetailViewController * __nonnull detailView;
- (void)viewDidLoad;
- (void)changeHand:(UISwitch * __nonnull)sender;
- (void)changeRightHand;
- (void)changeLeftHand;
- (void)reloadDetailView:(Element * __nonnull)element;
- (void)initCursors;
- (void)didTapOnCursor:(UIButton * __nonnull)sender;
- (void)didChangeValue:(UISlider * __nonnull)slider;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithNibName:(NSString * __nullable)nibNameOrNil bundle:(NSBundle * __nullable)nibBundleOrNil OBJC_DESIGNATED_INITIALIZER;
- (SWIFT_NULLABILITY(nonnull) instancetype)initWithCoder:(NSCoder * __nonnull)aDecoder OBJC_DESIGNATED_INITIALIZER;
@end

#pragma clang diagnostic pop
