// macOS only: the companion process is a background "accessory" app, so it has
// no Dock icon and showing the avatar never activates SMILE or brings its main
// window to the front.
#import <AppKit/AppKit.h>

void smileMakeAccessoryApp()
{
    [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];
}
