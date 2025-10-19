#define CLAY_IMPLEMENTATION
#include "clay.h"

float windowWidth = 1024, windowHeight = 768;
float modelPageOneZRotation = 0;
uint32_t ACTIVE_RENDERER_INDEX = 0;

const uint16_t FONT_ID_BARTLE = 0;
const uint16_t FONT_ID_BOGLE = 1;


#define DEFAULT_SPACING 16
#define DEFAULT_CORNER 10

const Clay_Color COLOR_LIGHT = (Clay_Color) {244, 235, 230, 255};
const Clay_Color COLOR_LIGHT_HOVER = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color) {209, 52, 52, 255};
const Clay_Color COLOR_RED_HOVER = (Clay_Color) {148, 46, 8, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};
const Clay_Color COLOR_GREEN = (Clay_Color) {56, 186, 47, 255};
const Clay_Color COLOR_BLUE = (Clay_Color) {52, 112, 168, 255};
const Clay_Color COLOR_BROWN = (Clay_Color) {61, 26, 5, 255};

const Clay_LayoutConfig defaultLayoutConfig = (Clay_LayoutConfig) { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) };

Clay_TextElementConfig titleTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 32, .textColor = COLOR_RED };
Clay_TextElementConfig headerTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 24, .textColor = COLOR_BROWN };
Clay_TextElementConfig blobTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 30, .textColor = COLOR_LIGHT };
Clay_TextElementConfig sideBarTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = COLOR_RED };

typedef struct {
    void* memory;
    uintptr_t offset;
} Arena;

Arena frameArena = {};

typedef struct {
    Clay_String link;
    bool cursorPointer;
    bool disablePointerEvents;
} CustomHTMLData;

typedef struct {
    float mousePositionX, mousePositionY;
    float deltaTime;
    bool isTouchDown, isMouseDown;
    bool isMousePressed, isMouseReleased;
} InputState;

InputState input;


CustomHTMLData* FrameAllocateCustomData(CustomHTMLData data) {
    CustomHTMLData *customData = (CustomHTMLData *)((char*)frameArena.memory + frameArena.offset);
    *customData = data;
    frameArena.offset += sizeof(CustomHTMLData);
    return customData;
}

Clay_String* FrameAllocateString(Clay_String string) {
    Clay_String *allocated = (Clay_String *)((char*)frameArena.memory + frameArena.offset);
    *allocated = string;
    frameArena.offset += sizeof(Clay_String);
    return allocated;
}


Clay_Color ColorLerp(Clay_Color a, Clay_Color b, float amount) {
    return (Clay_Color) {
        .r = a.r + (b.r - a.r) * amount,
        .g = a.g + (b.g - a.g) * amount,
        .b = a.b + (b.b - a.b) * amount,
        .a = a.a + (b.a - a.a) * amount,
    };
}

void HandleRendererButtonInteraction(Clay_ElementId elementId, Clay_PointerData pointerInfo, intptr_t userData) {
    if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
        ACTIVE_RENDERER_INDEX = (uint32_t)userData;
        Clay_SetCullingEnabled(ACTIVE_RENDERER_INDEX == 1);
        Clay_SetExternalScrollHandlingEnabled(ACTIVE_RENDERER_INDEX == 0);
    }
}

void RendererButtonActive(Clay_String text) {
    CLAY_AUTO_ID({
        .layout = { .sizing = {CLAY_SIZING_FIXED(300) }, .padding = CLAY_PADDING_ALL(16) },
        .backgroundColor = Clay_Hovered() ? COLOR_RED_HOVER : COLOR_RED,
        .cornerRadius = CLAY_CORNER_RADIUS(10),
        .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true, .cursorPointer = true })
    }) {
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 28, .fontId = FONT_ID_BARTLE, .textColor = COLOR_LIGHT }));
    }
}

void RendererButtonInactive(Clay_String text, size_t rendererIndex) {
    CLAY_AUTO_ID({
        .layout = { .sizing = {CLAY_SIZING_FIXED(300)}, .padding = CLAY_PADDING_ALL(16) },
        .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
        .backgroundColor = Clay_Hovered() ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
        .cornerRadius = CLAY_CORNER_RADIUS(10),
        .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true, .cursorPointer = true })
    }) {
        Clay_OnHover(HandleRendererButtonInteraction, rendererIndex);
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 28, .fontId = FONT_ID_BARTLE, .textColor = COLOR_RED }));
    }
}

typedef struct
{
    Clay_Vector2 clickOrigin;
    Clay_Vector2 positionOrigin;
    bool mouseDown;
} ScrollbarData;

typedef CLAY_PACKED_ENUM{
    PAGE_MAIN,
    PAGE_CHARACTERS,
    PAGE_CHARACTER_INFO,
    PAGE_CHARACTER_DRAFT,
    PAGE_IN_GAME
}Page;
Page current_page;

ScrollbarData scrollbarData = (ScrollbarData) {};
float animationLerpValue = -1.0f;
void SideBar(){
    sideBarTextConfig.userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true });
    CLAY(CLAY_ID("SideBar"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .childGap = DEFAULT_SPACING } }) {
        CLAY(CLAY_ID("Spacer_TOP"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } }) {}
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() || current_page == PAGE_MAIN ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_MAIN;
            CLAY_TEXT(CLAY_STRING("Home"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() || current_page == PAGE_CHARACTERS ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_CHARACTERS;
            CLAY_TEXT(CLAY_STRING("Characters"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() || current_page == PAGE_CHARACTER_DRAFT? COLOR_LIGHT_HOVER : COLOR_LIGHT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)
        }) {
            //if(Clay_Hovered()) current_page = PAGE_CHARACTER_DRAFT;
            CLAY_TEXT(CLAY_STRING("Battle"), &sideBarTextConfig);
        }
            CLAY(CLAY_ID("Spacer_BOT"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } }) {}
    }
}
void MainLayout(){
CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT, }){
            SideBar();
            CLAY(CLAY_ID("FILLER"), { .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = defaultLayoutConfig});
            CLAY(CLAY_ID("FILLER2"), { .backgroundColor = COLOR_ORANGE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout= defaultLayoutConfig});
        }
    }
}

void ChampionsLayout(){
CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT, }){
            SideBar();
            CLAY(CLAY_ID("FILLER"), { .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = defaultLayoutConfig});
            CLAY(CLAY_ID("FILLER2"), { .backgroundColor = COLOR_ORANGE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = defaultLayoutConfig});
        }
    }
}

void ChampionInfoLayout(){
    CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }, .backgroundColor = COLOR_LIGHT, }){
            SideBar();
            CLAY(CLAY_ID("FILLER"), { .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = defaultLayoutConfig});
            CLAY(CLAY_ID("FILLER2"), { .backgroundColor = COLOR_ORANGE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = defaultLayoutConfig});
        }
    }
}

void ChampionDraftLayout(){
}

void InGameLayout(){
}



Clay_RenderCommandArray CreateLayout(bool mobileScreen, float lerpValue) {
    Clay_BeginLayout();
    switch (current_page)
    {
        case PAGE_MAIN: MainLayout(); break;
        case PAGE_CHARACTERS: ChampionsLayout(); break;
        case PAGE_CHARACTER_INFO: ChampionInfoLayout(); break;
        case PAGE_CHARACTER_DRAFT: ChampionDraftLayout(); break;
        case PAGE_IN_GAME: InGameLayout(); break;
    }
    
    CLAY(CLAY_ID("OuterScrollContainer"), {
        .layout = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
        .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
        .border = { .width = { .betweenChildren = 2 }, .color = COLOR_RED }
    }) {
    }
    return Clay_EndLayout();
}

bool debugModeEnabled = false;

CLAY_WASM_EXPORT("SetScratchMemory") void SetScratchMemory(void * memory) {
    frameArena.memory = memory;
}

CLAY_WASM_EXPORT("UpdateDrawFrame") Clay_RenderCommandArray UpdateDrawFrame(float width, float height, float mouseWheelX, float mouseWheelY, float mousePositionX, float mousePositionY, bool isTouchDown, bool isMouseDown, bool arrowKeyDownPressedThisFrame, bool arrowKeyUpPressedThisFrame, bool dKeyPressedThisFrame, float deltaTime) {
    frameArena.offset = 0;
    windowWidth = width;
    windowHeight = height;
    Clay_SetLayoutDimensions((Clay_Dimensions) { width, height });
    Clay_ScrollContainerData scrollContainerData = Clay_GetScrollContainerData(Clay_GetElementId(CLAY_STRING("OuterScrollContainer")));
    Clay_LayoutElementHashMapItem *perfPage = Clay__GetHashMapItem(Clay_GetElementId(CLAY_STRING("PerformanceOuter")).id);
    // NaN propagation can cause pain here
    float perfPageYOffset = perfPage->boundingBox.y + scrollContainerData.scrollPosition->y;
    if (deltaTime == deltaTime && (ACTIVE_RENDERER_INDEX == 1 || (perfPageYOffset < height && perfPageYOffset + perfPage->boundingBox.height > 0))) {
        animationLerpValue += deltaTime;
        if (animationLerpValue > 1) {
            animationLerpValue -= 2;
        }
    }

    if (dKeyPressedThisFrame) {
        debugModeEnabled = !debugModeEnabled;
        Clay_SetDebugModeEnabled(debugModeEnabled);
    }
    Clay_SetCullingEnabled(ACTIVE_RENDERER_INDEX == 1);
    Clay_SetExternalScrollHandlingEnabled(ACTIVE_RENDERER_INDEX == 0);

    Clay__debugViewHighlightColor = (Clay_Color) {105,210,231, 120};

    Clay_SetPointerState((Clay_Vector2) {mousePositionX, mousePositionY}, isMouseDown || isTouchDown);

    if (!isMouseDown) {
        scrollbarData.mouseDown = false;
    }

    if (isMouseDown && !scrollbarData.mouseDown && Clay_PointerOver(Clay_GetElementId(CLAY_STRING("ScrollBar")))) {
        scrollbarData.clickOrigin = (Clay_Vector2) { mousePositionX, mousePositionY };
        scrollbarData.positionOrigin = *scrollContainerData.scrollPosition;
        scrollbarData.mouseDown = true;
    } else if (scrollbarData.mouseDown) {
        if (scrollContainerData.contentDimensions.height > 0) {
            Clay_Vector2 ratio = (Clay_Vector2) {
                scrollContainerData.contentDimensions.width / scrollContainerData.scrollContainerDimensions.width,
                scrollContainerData.contentDimensions.height / scrollContainerData.scrollContainerDimensions.height,
            };
            if (scrollContainerData.config.vertical) {
                scrollContainerData.scrollPosition->y = scrollbarData.positionOrigin.y + (scrollbarData.clickOrigin.y - mousePositionY) * ratio.y;
            }
            if (scrollContainerData.config.horizontal) {
                scrollContainerData.scrollPosition->x = scrollbarData.positionOrigin.x + (scrollbarData.clickOrigin.x - mousePositionX) * ratio.x;
            }
        }
    }

    if (arrowKeyDownPressedThisFrame) {
        if (scrollContainerData.contentDimensions.height > 0) {
            scrollContainerData.scrollPosition->y = scrollContainerData.scrollPosition->y - 50;
        }
    } else if (arrowKeyUpPressedThisFrame) {
        if (scrollContainerData.contentDimensions.height > 0) {
            scrollContainerData.scrollPosition->y = scrollContainerData.scrollPosition->y + 50;
        }
    }

    Clay_UpdateScrollContainers(isTouchDown, (Clay_Vector2) {mouseWheelX, mouseWheelY}, deltaTime);
    bool isMobileScreen = windowWidth < 750;
    if (debugModeEnabled) {
        isMobileScreen = windowWidth < 950;
    }

    input.deltaTime = deltaTime;
    input.isMousePressed = !input.isMouseDown && isMouseDown;
    input.isMouseReleased = input.isMouseDown && !isMouseDown;
    input.isMouseDown = isMouseDown;
    input.isTouchDown = isTouchDown;
    input.mousePositionX = mousePositionX;
    input.mousePositionY = mousePositionY;

    return CreateLayout(isMobileScreen, animationLerpValue < 0 ? (animationLerpValue + 1) : (1 - animationLerpValue));
    //----------------------------------------------------------------------------------
}

// Dummy main() to please cmake - TODO get wasm working with cmake on this example
int main() {
    return 0;
}
