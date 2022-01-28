#include "imgui.h"
#include "imgui_impl_metal.h"
#include <stdio.h>
#include "node_editor.h"
#include <imnodes.h>
#include "imgui_impl_osx.h"
#include <crt_externs.h>
#import <Metal/Metal.h>
#import <Cocoa/Cocoa.h>
#import <MetalKit/MetalKit.h>

namespace MacOSCode{
    char* openFile(){
        @autoreleasepool {
            id panl = [NSOpenPanel openPanel];
            [panl setAllowsMultipleSelection:NO];
            [panl setCanChooseDirectories:NO];
            [panl setCanCreateDirectories:YES];
            [panl setCanChooseFiles:YES];
            if([panl runModal]==NSModalResponseOK){
                NSString* filepath = [[[panl URLs] firstObject] path];
                if(filepath){
                    if([filepath length] > 0){
                        char* f = new char[[filepath length]]();
                        memcpy(f, [filepath cStringUsingEncoding:NSUTF8StringEncoding], [filepath length]);
                        return f;
                    }else{
                        return NULL;
                    }
                }else{
                    return NULL;
                }
            }else{
                return NULL;
            }
        }
    }
}

@interface AppViewController : NSViewController
@end

@interface AppViewController () <MTKViewDelegate>
@property (nonatomic, readonly) MTKView *mtkView;
@property (nonatomic, strong) id <MTLDevice> device;
@property (nonatomic, strong) id <MTLCommandQueue> commandQueue;
@end

//-----------------------------------------------------------------------------------
// AppViewController
//-----------------------------------------------------------------------------------

@implementation AppViewController

-(instancetype)initWithNibName:(nullable NSString *)nibNameOrNil bundle:(nullable NSBundle *)nibBundleOrNil
{
    self = [super initWithNibName:nibNameOrNil bundle:nibBundleOrNil];

    _device = MTLCreateSystemDefaultDevice();
    _commandQueue = [_device newCommandQueue];

    if (!self.device)
    {
        NSLog(@"Metal is not supported");
        abort();
    }

    // Setup Dear ImGui context
    // FIXME: This example doesn't have proper cleanup...
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImNodes::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Renderer backend
    ImGui_ImplMetal_Init(_device);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Read 'docs/FONTS.txt' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
//    IM_ASSERT(font != NULL);
    io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/PingFang.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());

    return self;
}

-(MTKView *)mtkView
{
    return (MTKView *)self.view;
}

-(void)loadView
{
    self.view = [[MTKView alloc] initWithFrame:CGRectMake(0, 0, 1200, 720)];
}

-(void)viewDidLoad
{
    [super viewDidLoad];

    self.mtkView.device = self.device;
    self.mtkView.delegate = self;

    // Add a tracking area in order to receive mouse events whenever the mouse is within the bounds of our view
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc] initWithRect:NSZeroRect
                                                                options:NSTrackingMouseMoved | NSTrackingInVisibleRect | NSTrackingActiveAlways
                                                                  owner:self
                                                               userInfo:nil];
    [self.view addTrackingArea:trackingArea];

    // If we want to receive key events, we either need to be in the responder chain of the key view,
    // or else we can install a local monitor. The consequence of this heavy-handed approach is that
    // we receive events for all controls, not just Dear ImGui widgets. If we had native controls in our
    // window, we'd want to be much more careful than just ingesting the complete event stream.
    // To match the behavior of other backends, we pass every event down to the OS.
    NSEventMask eventMask = NSEventMaskKeyDown | NSEventMaskKeyUp | NSEventMaskFlagsChanged;
    [NSEvent addLocalMonitorForEventsMatchingMask:eventMask handler:^NSEvent * _Nullable(NSEvent *event)
    {
        ImGui_ImplOSX_HandleEvent(event, self.view);
        return event;
    }];

    ImGui_ImplOSX_Init(self.view);
    example::NodeEditorInitialize(*_NSGetArgc(), *_NSGetArgv());
}

-(void)drawInMTKView:(MTKView*)view
{
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize.x = view.bounds.size.width;
    io.DisplaySize.y = view.bounds.size.height;

    CGFloat framebufferScale = view.window.screen.backingScaleFactor ?: NSScreen.mainScreen.backingScaleFactor;
    io.DisplayFramebufferScale = ImVec2(framebufferScale, framebufferScale);

    io.DeltaTime = 1 / float(view.preferredFramesPerSecond ?: 60);

    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];

    MTLRenderPassDescriptor* renderPassDescriptor = view.currentRenderPassDescriptor;
    if (renderPassDescriptor == nil)
    {
        [commandBuffer commit];
		return;
    }

    // Start the Dear ImGui frame
    ImGui_ImplMetal_NewFrame(renderPassDescriptor);
    ImGui_ImplOSX_NewFrame(view);
    ImGui::NewFrame();

    // Our state (make them static = more or less global) as a convenience to keep the example terse.
    static ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
    example::NodeEditorShow();

    // Rendering
    ImGui::Render();
    ImDrawData* draw_data = ImGui::GetDrawData();

    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
    id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    [renderEncoder pushDebugGroup:@"Dear ImGui rendering"];
    ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, renderEncoder);
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];

	// Present
    [commandBuffer presentDrawable:view.currentDrawable];
    [commandBuffer commit];
}

-(void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size
{
}

//-----------------------------------------------------------------------------------
// Input processing
//-----------------------------------------------------------------------------------


// Forward Mouse/Keyboard events to Dear ImGui OSX backend.
// Other events are registered via addLocalMonitorForEventsMatchingMask()
-(void)mouseDown:(NSEvent *)event           { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)rightMouseDown:(NSEvent *)event      { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)otherMouseDown:(NSEvent *)event      { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)mouseUp:(NSEvent *)event             { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)rightMouseUp:(NSEvent *)event        { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)otherMouseUp:(NSEvent *)event        { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)mouseMoved:(NSEvent *)event          { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)mouseDragged:(NSEvent *)event        { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)rightMouseMoved:(NSEvent *)event     { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)rightMouseDragged:(NSEvent *)event   { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)otherMouseMoved:(NSEvent *)event     { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)otherMouseDragged:(NSEvent *)event   { ImGui_ImplOSX_HandleEvent(event, self.view); }
-(void)scrollWheel:(NSEvent *)event         { ImGui_ImplOSX_HandleEvent(event, self.view); }



@end

//-----------------------------------------------------------------------------------
// AppDelegate
//-----------------------------------------------------------------------------------

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (nonatomic, strong) NSWindow *window;
@end

@implementation AppDelegate

-(BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

-(instancetype)init
{
    if (self = [super init])
    {
        NSViewController *rootViewController = [[AppViewController alloc] initWithNibName:nil bundle:nil];
        self.window = [[NSWindow alloc] initWithContentRect:NSZeroRect
                                                  styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
        self.window.contentViewController = rootViewController;
        [self.window orderFront:self];
        [self.window center];
        [self.window becomeKeyWindow];
    }
    return self;
}

@end

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        id d =[AppDelegate new];
        id app = [NSApplication sharedApplication];
        [app setDelegate:d];
        [app run];
    }
    return 0;
}

// int main(int argc, char* argv[])
// {
//     bool initialized = false;
//     // Setup Dear ImGui context
//     IMGUI_CHECKVERSION();
//     ImGui::CreateContext();
//     ImGuiIO& io = ImGui::GetIO(); (void)io;
//     //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
//     //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
//     ImNodes::CreateContext();
//     // Setup style
//     ImGui::StyleColorsDark();
//     //ImGui::StyleColorsClassic();

//     // Load Fonts
//     // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
//     // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
//     // - If the file cannot be loaded, the function will return NULL. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
//     // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
//     // - Read 'docs/FONTS.txt' for more instructions and details.
//     // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
// //    io.Fonts->AddFontDefault();
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
//     //io.Fonts->AddFontFromFileTTF("../../misc/fonts/ProggyTiny.ttf", 10.0f);
//     //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, NULL, io.Fonts->GetGlyphRangesJapanese());
//     //IM_ASSERT(font != NULL);
//     io.Fonts->AddFontFromFileTTF("/System/Library/Fonts/PingFang.ttc", 15.0f, NULL, io.Fonts->GetGlyphRangesChineseFull());
//     // Setup SDL
//     // (Some versions of SDL before <2.0.10 appears to have performance/stalling issues on a minority of Windows systems,
//     // depending on whether SDL_INIT_GAMECONTROLLER is enabled or disabled.. updating to latest version of SDL is recommended!)
//     if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
//     {
//         printf("Error: %s\n", SDL_GetError());
//         return -1;
//     }

//     // Inform SDL that we will be using metal for rendering. Without this hint initialization of metal renderer may fail.
//     SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");

//     SDL_Window* window = SDL_CreateWindow("Neural Net Editor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
//     if (window == NULL)
//     {
//         printf("Error creating window: %s\n", SDL_GetError());
//         return -2;
//     }

//     SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
//     if (renderer == NULL)
//     {
//         printf("Error creating renderer: %s\n", SDL_GetError());
//         return -3;
//     }

//     // Setup Platform/Renderer backends
//     CAMetalLayer* layer = (__bridge CAMetalLayer*)SDL_RenderGetMetalLayer(renderer);
//     layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
//     ImGui_ImplMetal_Init(layer.device);
//     ImGui_ImplSDL2_InitForMetal(window);

//     id<MTLCommandQueue> commandQueue = [layer.device newCommandQueue];
//     MTLRenderPassDescriptor* renderPassDescriptor = [MTLRenderPassDescriptor new];

//     // Our state
//     bool show_demo_window = true;
//     bool show_another_window = false;
//     float clear_color[4] = {0.45f, 0.55f, 0.60f, 1.00f};
//     example::NodeEditorInitialize(argc, argv);

//     // Main loop
//     bool done = false;
//     while (!done)
//     {
//         @autoreleasepool
//         {
//             // Poll and handle events (inputs, window resize, etc.)
//             // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
//             // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application.
//             // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application.
//             // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
//             SDL_Event event;
//             while (SDL_PollEvent(&event))
//             {
//                 ImGui_ImplSDL2_ProcessEvent(&event);
//                 example::NodeEditorSetEvent(&event);
//                 if (event.type == SDL_QUIT)
//                     done = true;
//                 if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
//                     done = true;
//             }

//             int width, height;
//             SDL_GetRendererOutputSize(renderer, &width, &height);
//             layer.drawableSize = CGSizeMake(width, height);
//             id<CAMetalDrawable> drawable = [layer nextDrawable];

//             id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
//             renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(clear_color[0] * clear_color[3], clear_color[1] * clear_color[3], clear_color[2] * clear_color[3], clear_color[3]);
//             renderPassDescriptor.colorAttachments[0].texture = drawable.texture;
//             renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
//             renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
//             id <MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
//             [renderEncoder pushDebugGroup:@"ImGui demo"];

//             // Start the Dear ImGui frame
//             ImGui_ImplMetal_NewFrame(renderPassDescriptor);
//             ImGui_ImplSDL2_NewFrame();
//             ImGui::NewFrame();
//         example::NodeEditorShow();

//             // Rendering
//             ImGui::Render();
//             ImGui_ImplMetal_RenderDrawData(ImGui::GetDrawData(), commandBuffer, renderEncoder);

//             [renderEncoder popDebugGroup];
//             [renderEncoder endEncoding];

//             [commandBuffer presentDrawable:drawable];
//             [commandBuffer commit];
//         }
//     }

//     // Cleanup
//     ImGui_ImplMetal_Shutdown();
//     ImGui_ImplSDL2_Shutdown();
//     ImGui::DestroyContext();

//     SDL_DestroyRenderer(renderer);
//     SDL_DestroyWindow(window);
//     SDL_Quit();

//     return 0;
// }
