#define CLAY_IMPLEMENTATION
#include "clay.h"

float windowWidth = 1024, windowHeight = 768;
float modelPageOneZRotation = 0;
uint32_t ACTIVE_RENDERER_INDEX = 0;

const uint16_t FONT_ID_TITLE_56 = 1;
const uint16_t FONT_ID_BODY_24 = 2;
const uint16_t FONT_ID_BODY_36 = 3;
const uint16_t FONT_ID_TITLE_36 = 4;

const Clay_Color COLOR_LIGHT = (Clay_Color) {244, 235, 230, 255};
const Clay_Color COLOR_LIGHT_HOVER = (Clay_Color) {224, 215, 210, 255};
const Clay_Color COLOR_RED = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_RED_HOVER = (Clay_Color) {148, 46, 8, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};
//const Clay_Color COLOR_BLUE = (Clay_Color) {111, 173, 162, 255};

// Colors for top stripe
const Clay_Color COLOR_TOP_BORDER_1 = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_TOP_BORDER_2 = (Clay_Color) {223, 110, 44, 255};
const Clay_Color COLOR_TOP_BORDER_3 = (Clay_Color) {225, 138, 50, 255};
const Clay_Color COLOR_TOP_BORDER_4 = (Clay_Color) {236, 189, 80, 255};
const Clay_Color COLOR_TOP_BORDER_5 = (Clay_Color) {240, 213, 137, 255};

const Clay_Color COLOR_BLOB_BORDER_1 = (Clay_Color) {168, 66, 28, 255};
const Clay_Color COLOR_BLOB_BORDER_2 = (Clay_Color) {203, 100, 44, 255};
const Clay_Color COLOR_BLOB_BORDER_3 = (Clay_Color) {225, 138, 50, 255};
const Clay_Color COLOR_BLOB_BORDER_4 = (Clay_Color) {236, 159, 70, 255};
const Clay_Color COLOR_BLOB_BORDER_5 = (Clay_Color) {240, 189, 100, 255};

#define RAYLIB_VECTOR2_TO_CLAY_VECTOR2(vector) (Clay_Vector2) { .x = (vector).x, .y = (vector).y }
Clay_TextElementConfig titleTextConfig = (Clay_TextElementConfig) { .fontId = 2, .fontSize = 32, .textColor = {61, 26, 5, 255}, .wrapMode = CLAY_TEXT_WRAP_NONE };
Clay_TextElementConfig headerTextConfig = (Clay_TextElementConfig) { .fontId = 2, .fontSize = 24, .textColor = {61, 26, 5, 255} };
Clay_TextElementConfig blobTextConfig = (Clay_TextElementConfig) { .fontId = 2, .fontSize = 30, .textColor = {244, 235, 230, 255} };

typedef struct {
    void* memory;
    uintptr_t offset;
} Arena;

Arena frameArena = {};

typedef struct d {
    Clay_String link;
    bool cursorPointer;
    bool disablePointerEvents;
} CustomHTMLData;

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

Clay_String LOREM_IPSUM_TEXT = CLAY_STRING("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.");

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
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 28, .fontId = FONT_ID_BODY_36, .textColor = COLOR_LIGHT }));
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
        CLAY_TEXT(text, CLAY_TEXT_CONFIG({ .fontSize = 28, .fontId = FONT_ID_BODY_36, .textColor = COLOR_RED }));
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
    PAGE_CHAMPIONS,
    PAGE_CHAMPION_INFO,
    PAGE_CHAMPION_DRAFT,
    PAGE_IN_GAME
}Page;
Page current_page;

ScrollbarData scrollbarData = (ScrollbarData) {};
float animationLerpValue = -1.0f;

void MainLayout(){
CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) } }, .backgroundColor = COLOR_LIGHT }) {
        CLAY(CLAY_ID("Header"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIXED(100), CLAY_SIZING_GROW(0) }, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .childGap = 16, .padding = { 32, 32, 16 } } }) {
            CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
            CLAY(CLAY_ID("Spacer_TOP"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } }) {}
            CLAY_AUTO_ID({
                .layout = { .padding = {16, 16, 6, 6} },
                .backgroundColor = Clay_Hovered() ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
                .cornerRadius = CLAY_CORNER_RADIUS(10),
                .userData = FrameAllocateCustomData((CustomHTMLData) { .link = CLAY_STRING("https://discord.gg/b4FTWkxdvT") }),
            }) {
                CLAY_TEXT(CLAY_STRING("Discord"), CLAY_TEXT_CONFIG({
                    .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }),
                    .fontId = FONT_ID_BODY_24, .fontSize = 24, .textColor = {61, 26, 5, 255} }));
            }
            CLAY_AUTO_ID({
                .layout = { .padding = {16, 16, 6, 6} },
                .backgroundColor = Clay_Hovered() ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
                .cornerRadius = CLAY_CORNER_RADIUS(10),
                .userData = FrameAllocateCustomData((CustomHTMLData) { .link = CLAY_STRING("https://discord.gg/b4FTWkxdvT") }),
            }) {
                CLAY_TEXT(CLAY_STRING("Discord"), CLAY_TEXT_CONFIG({
                    .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }),
                    .fontId = FONT_ID_BODY_24, .fontSize = 24, .textColor = {61, 26, 5, 255} }));
            }
            CLAY_AUTO_ID({
                .layout = { .padding = {16, 16, 6, 6} },
                .backgroundColor = Clay_Hovered() ? COLOR_LIGHT_HOVER : COLOR_LIGHT,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_RED },
                .cornerRadius = CLAY_CORNER_RADIUS(10),
                .userData = FrameAllocateCustomData((CustomHTMLData) { .link = CLAY_STRING("https://github.com/nicbarker/clay") }),
            }) {
                CLAY_TEXT(CLAY_STRING("Github"), CLAY_TEXT_CONFIG({
                    .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }),
                    .fontId = FONT_ID_BODY_24, .fontSize = 24, .textColor = {61, 26, 5, 255} }));
            }
            CLAY(CLAY_ID("Spacer_BOT"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } }) {}
        }
    }
}

void ChampionsLayout(){

}

void ChampionInfoLayout(){
    
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
        case PAGE_CHAMPIONS: ChampionsLayout(); break;
        case PAGE_CHAMPION_INFO: ChampionInfoLayout(); break;
        case PAGE_CHAMPION_DRAFT: ChampionDraftLayout(); break;
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
    return CreateLayout(isMobileScreen, animationLerpValue < 0 ? (animationLerpValue + 1) : (1 - animationLerpValue));
    //----------------------------------------------------------------------------------
}

// Dummy main() to please cmake - TODO get wasm working with cmake on this example
int main() {
    return 0;
}
