#define CLAY_IMPLEMENTATION
#include "clay.h"

float windowWidth = 1024, windowHeight = 768;
float modelPageOneZRotation = 0;
uint32_t ACTIVE_RENDERER_INDEX = 0;

const uint16_t FONT_ID_BARTLE = 0;
const uint16_t FONT_ID_BOGLE = 1;


#define DEFAULT_SPACING 16
#define DEFAULT_CORNER 10

const Clay_Color COLOR_TRANSPERENT = (Clay_Color) {0, 0, 0, 0};
const Clay_Color COLOR_LIGHT = (Clay_Color) {244, 235, 230, 255};
const Clay_Color COLOR_LIGHT_HOVER = (Clay_Color) {200, 180, 180, 255};
const Clay_Color COLOR_RED = (Clay_Color) {209, 52, 52, 255};
const Clay_Color COLOR_RED_HOVER = (Clay_Color) {125, 35, 35, 255};
const Clay_Color COLOR_ORANGE = (Clay_Color) {225, 138, 50, 255};
const Clay_Color COLOR_GREEN = (Clay_Color) {56, 186, 47, 255};
const Clay_Color COLOR_BLUE = (Clay_Color) {20, 56, 115, 230};
const Clay_Color COLOR_BROWN = (Clay_Color) {61, 26, 5, 255};

const Clay_LayoutConfig defaultLayoutConfig = (Clay_LayoutConfig) { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) };
const Clay_LayoutConfig fitLayoutConfig = (Clay_LayoutConfig) { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) };

Clay_TextElementConfig titleTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 42, .textColor = COLOR_RED };
Clay_TextElementConfig sideBarTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = COLOR_LIGHT };
Clay_TextElementConfig headerTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 36, .textColor = COLOR_LIGHT };
Clay_TextElementConfig defaultTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 28, .textColor = COLOR_LIGHT };
Clay_TextElementConfig smallTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT_HOVER};

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
    PAGE_CHARACTER_DRAFT,
    PAGE_IN_GAME   
}Page;
Page current_page;

ScrollbarData scrollbarData = (ScrollbarData) {};
float animationLerpValue = -1.0f;
void SideBar(){
    sideBarTextConfig.userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true });
    CLAY(CLAY_ID("SideBar"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, 
    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .padding = { 0, DEFAULT_SPACING, 0, 0, }, .childGap = DEFAULT_SPACING }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT } }) {
        CLAY(CLAY_ID("Spacer_TOP"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_MAIN? COLOR_RED_HOVER : COLOR_TRANSPERENT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_MAIN;
            CLAY_TEXT(CLAY_STRING("Home"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_CHARACTERS ? COLOR_RED_HOVER : COLOR_TRANSPERENT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_CHARACTERS;
            CLAY_TEXT(CLAY_STRING("Characters"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_CHARACTER_DRAFT ? COLOR_RED_HOVER : COLOR_TRANSPERENT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_CHARACTER_DRAFT;
            CLAY_TEXT(CLAY_STRING("Battle"), &sideBarTextConfig);
        }
            CLAY(CLAY_ID("Spacer_BOT"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
    }
}

Clay_String background_image = CLAY_STRING("images/background.jpg");
Clay_String mainpage_image = CLAY_STRING("images/mainpage.png");
void MainLayout(){
CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = {DEFAULT_SPACING, 0, DEFAULT_SPACING, DEFAULT_SPACING, }, .childGap = DEFAULT_SPACING }, .image = {&background_image} }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
            SideBar();
            CLAY(CLAY_ID("ContentContainer"), { .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                CLAY(CLAY_ID("MAINPAGE_IMAGE"), { .image = { .imageData = &mainpage_image}, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .aspectRatio = {16.0f/9.0f},
                .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .padding = {DEFAULT_SPACING, DEFAULT_SPACING, DEFAULT_SPACING, 60 }, },
                .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_RIGHT_BOTTOM, CLAY_ATTACH_POINT_RIGHT_BOTTOM}}});
                CLAY(CLAY_ID("MISSIONS"), { .backgroundColor = COLOR_BLUE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), 
                .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIXED(500), CLAY_SIZING_FIXED(400) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }, 
                .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_LEFT_BOTTOM, CLAY_ATTACH_POINT_LEFT_BOTTOM}}}){
                    CLAY_TEXT(CLAY_STRING("Missions"), &headerTextConfig);
                    CLAY_AUTO_ID({
                        .layout = defaultLayoutConfig,
                        .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
                        .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
                    }) {
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(0) }}}){
                            CLAY_TEXT(CLAY_STRING("First Win of the Day"), &defaultTextConfig);
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = {DEFAULT_SPACING, 0, 0, 0 }}}){
                               CLAY_TEXT(CLAY_STRING("Win an online Battle"), &smallTextConfig); 
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(CLAY_STRING("Progress: 0/1"), &smallTextConfig);
                            }
                        }
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } }, .border = { .width = {0, 0, 0, 2}, .color = COLOR_LIGHT }});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(0) }}}){
                            CLAY_TEXT(CLAY_STRING("Head Hunter"), &defaultTextConfig);
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = {DEFAULT_SPACING, 0, 0, 0 }}}){
                               CLAY_TEXT(CLAY_STRING("Kill 20 enemies"), &smallTextConfig); 
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(CLAY_STRING("Progress: 0/20"), &smallTextConfig);
                            }
                        }
                        CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } }, .border = { .width = {0, 0, 0, 2}, .color = COLOR_LIGHT }});
                                                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(0) }}}){
                            CLAY_TEXT(CLAY_STRING("Demolition"), &defaultTextConfig);
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = {DEFAULT_SPACING, 0, 0, 0 }}}){
                               CLAY_TEXT(CLAY_STRING("Destroy 5 towers in one game"), &smallTextConfig); 
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(CLAY_STRING("Progress: 0/1"), &smallTextConfig);
                            }
                        }
                    }
                }
            }
        }
    }
}

Clay_String alysia_icon = CLAY_STRING("images/alysia_icon.jpg");
Clay_String alysia_image = CLAY_STRING("images/Alysia.png");
Clay_String ashka_icon = CLAY_STRING("images/ashka_icon.jpg");
Clay_String bakko_icon = CLAY_STRING("images/bakko_icon.jpg");
Clay_String blossom_icon = CLAY_STRING("images/blossom_icon.jpg");
Clay_String croak_icon = CLAY_STRING("images/croak_icon.jpg");
Clay_String destiny_icon = CLAY_STRING("images/destiny_icon.jpg");
Clay_String ezmo_icon = CLAY_STRING("images/ezmo_icon.jpg");
Clay_String freya_icon = CLAY_STRING("images/freya_icon.jpg");
Clay_String iva_icon = CLAY_STRING("images/iva_icon.jpg");
Clay_String jade_icon = CLAY_STRING("images/jade_icon.jpg");
Clay_String jamila_icon = CLAY_STRING("images/jamila_icon.jpg");
Clay_String jumong_icon = CLAY_STRING("images/jumong_icon.jpg");
Clay_String lucie_icon = CLAY_STRING("images/lucie_icon.jpg");
Clay_String raigon_icon = CLAY_STRING("images/raigon_icon.jpg");
Clay_String sirius_icon = CLAY_STRING("images/sirius_icon.jpg");

typedef enum :uint8_t{
    CHARACTER_NONE,
    CHARACTER_ALYSIA,
    CHARACTER_ASHKA,
    CHARACTER_BAKKO,
    CHARACTER_BLOSSOM,
    CHARACTER_CROAK,
    CHARACTER_DESTINY,
    CHARACTER_EZMO,
    CHARACTER_FREYA,
    CHARACTER_IVA,
    CHARACTER_JADE,
    CHARACTER_JAMILA,
    CHARACTER_JUMONG,
    CHARACTER_LUCIE,
    CHARACTER_RAIGON,
    CHARACTER_SIRIUS,
}Characters;
Characters characterInfoSelect = CHARACTER_NONE;

void CharactersContainer(){
    CLAY(CLAY_ID("Container"), { .layout =  { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }}){
        CLAY(CLAY_ID("Row1"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_ALYSIA;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &alysia_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Alysia"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_ASHKA;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &ashka_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Ashka"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_BAKKO;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &bakko_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Bakko"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_BLOSSOM;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &blossom_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Blossom"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_CROAK;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &croak_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Croak"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_DESTINY;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &destiny_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Destiny"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_EZMO;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &ezmo_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Ezmo"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
        }
        CLAY(CLAY_ID("Row2"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING }}){
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_FREYA;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &freya_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Freya"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_IVA;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &iva_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Iva"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_JADE;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &jade_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Jade"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_JAMILA;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &jamila_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Jamila"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_JUMONG;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &jumong_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Jumong"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_LUCIE;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &lucie_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Lucie"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_RAIGON;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &raigon_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Raigon"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
        }
        CLAY(CLAY_ID("Row3"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_FIT(.min = 1636), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING }}){
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    characterInfoSelect = CHARACTER_SIRIUS;
                }
                CLAY_AUTO_ID({ .image = { .imageData = &sirius_icon},
                .layout = {.sizing = { CLAY_SIZING_FIXED(220), CLAY_SIZING_FIXED(120) }},
                .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Sirius"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                    .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                    CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
            }}
        }
    }
}

Clay_String alysia_ability1 = CLAY_STRING("images/alysia_ability1.png");
Clay_String alysia_ability2 = CLAY_STRING("images/alysia_ability2.png");
Clay_String alysia_ability3 = CLAY_STRING("images/alysia_ability3.png");
Clay_String alysia_ability4 = CLAY_STRING("images/alysia_ability4.png");
void AlysiaCharacterInfo(){
    CLAY(CLAY_ID("AlysiaContainer"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }}){
        CLAY(CLAY_ID("Name"), { 
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}, 
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_RIGHT_TOP, CLAY_ATTACH_POINT_CENTER_TOP}}}){
            CLAY_TEXT(CLAY_STRING("ALYSIA"), &headerTextConfig);
        }
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
            CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() ? COLOR_RED : COLOR_TRANSPERENT,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
            }) {
                if(Clay_Hovered() && input.isMouseReleased) characterInfoSelect = CHARACTER_NONE;
                CLAY_TEXT(CLAY_STRING("Back"), &sideBarTextConfig);
            }
        }
        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_PERCENT(0.40f), CLAY_SIZING_GROW(0) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}){
            CLAY(CLAY_ID("ALYSIA_IMAGE"), { .image = { .imageData = &alysia_image}, .aspectRatio = {600.0f/800.0f},
            .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
        }
        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 800), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER}}}){
                CLAY_TEXT(CLAY_STRING("ABILITES"), &headerTextConfig);
            }
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 800), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25)}){
                CLAY(CLAY_ID("ALYSIA_ABILITY1"), { .image = { .imageData = &alysia_ability1},
                .layout = {.sizing = { CLAY_SIZING_FIXED(128), CLAY_SIZING_FIXED(128) }}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, 
                .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
                    CLAY_TEXT(CLAY_STRING("Frost Bolt"), &sideBarTextConfig);
                    CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Launch a cold bolt that deals damage.\nDeals bonus damage and adds Chill duration to enemies affected by Chill ."), 
                    &smallTextConfig);
                }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 800), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25)}){
                CLAY(CLAY_ID("ALYSIA_ABILITY2"), { .image = { .imageData = &alysia_ability2},
                .layout = {.sizing = { CLAY_SIZING_FIXED(128), CLAY_SIZING_FIXED(128) }}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, 
                .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
                    CLAY_TEXT(CLAY_STRING("Ice Lance"), &sideBarTextConfig);
                    CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Projectile attack that deals damage and inflicts Chill .\nDeals bonus damage to enemies already affected by Chill ."), 
                    &smallTextConfig);
                }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 800), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25)}){
                CLAY(CLAY_ID("ALYSIA_ABILITY3"), { .image = { .imageData = &alysia_ability3},
                .layout = {.sizing = { CLAY_SIZING_FIXED(128), CLAY_SIZING_FIXED(128) }}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, 
                .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
                    CLAY_TEXT(CLAY_STRING("Arctic Wind"), &sideBarTextConfig);
                    CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Soar gracefully to target location ."), 
                    &smallTextConfig);
                }}
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 800), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25)}){
                CLAY(CLAY_ID("ALYSIA_ABILITY4"), { .image = { .imageData = &alysia_ability4},
                .layout = {.sizing = { CLAY_SIZING_FIXED(128), CLAY_SIZING_FIXED(128) }}});
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, 
                .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
                    CLAY_TEXT(CLAY_STRING("Frozen Gallery"), &sideBarTextConfig);
                    CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
                    CLAY_TEXT(CLAY_STRING("Mark a path of frost in front of you. After a delay, the patch deals damage and inflicts Freeze to enemies hit, turning them into beautiful statues."), 
                    &smallTextConfig);
                }}
        }
    }
}


void ChampionsLayout(){
    CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .image = {&background_image} }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
            SideBar();
            switch (characterInfoSelect)
            {
            case CHARACTER_NONE: CharactersContainer(); break;
            case CHARACTER_ALYSIA: AlysiaCharacterInfo(); break;
            default: CharactersContainer(); break;
            }
        }
    }
}

Characters characterDraftSelect = CHARACTER_NONE;
void ChampionDraftLayout(){
    CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .image = {&background_image} }) {
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
            SideBar();
            CLAY(CLAY_ID("CHARACTERS"), {.cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, 
            .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }}){
                CLAY(CLAY_ID("Row1"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_ALYSIA;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &alysia_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Alysia"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 28, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_ASHKA;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &ashka_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Ashka"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_BAKKO;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &bakko_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Bakko"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_BLOSSOM;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &blossom_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Blossom"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    } 
                }
                CLAY(CLAY_ID("Row2"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_CROAK;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &croak_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Croak"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_DESTINY;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &destiny_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Destiny"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_EZMO;
                        }
                    CLAY_AUTO_ID({ .image = { .imageData = &ezmo_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Ezmo"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_FREYA;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &freya_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Freya"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                }
                CLAY(CLAY_ID("Row3"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_IVA;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &iva_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Iva"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_JADE;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &jade_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Jade"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_JAMILA;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &jamila_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Jamila"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = CHARACTER_JUMONG;
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &jumong_icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120), CLAY_SIZING_FIXED(65) }},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(CLAY_STRING("Jumong"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                    }
                }
            }
            CLAY(CLAY_ID("PLAY_BUTTON"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}, 
            .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {0, -50}}
            })
            {
                CLAY_AUTO_ID({ .layout = { .padding = {32, 32, 16, 16} },
                .backgroundColor = Clay_Hovered()? COLOR_RED : COLOR_RED_HOVER,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
                .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}) 
                {
                    if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_IN_GAME;
                        CLAY_TEXT(CLAY_STRING("BATTLE"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BARTLE, .fontSize = 48, .textColor = COLOR_LIGHT, .userData =  FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
            }
            //CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_FIXED(0) } } });
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = {DEFAULT_SPACING, 100, DEFAULT_SPACING, DEFAULT_SPACING}, .childAlignment = { .x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_CENTER}}}){
                //CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
                void* image = NULL;
                switch (characterDraftSelect)
                {
                    case CHARACTER_NONE: image = NULL; break;
                    case CHARACTER_ALYSIA: break;
                    case CHARACTER_ASHKA: break;
                    case CHARACTER_BAKKO: break;
                    case CHARACTER_BLOSSOM: break;
                    case CHARACTER_CROAK: break;
                    case CHARACTER_DESTINY: break;
                    case CHARACTER_EZMO: break;
                    case CHARACTER_FREYA: break;
                    case CHARACTER_IVA: break;
                    case CHARACTER_JADE: break;
                    case CHARACTER_JAMILA: break;
                    case CHARACTER_JUMONG: break;
                    case CHARACTER_LUCIE: break;
                    case CHARACTER_RAIGON: break;
                    case CHARACTER_SIRIUS: break;
                }
                CLAY(CLAY_ID("CHAR_IMAGE"), { .image = { .imageData = &alysia_image}, .aspectRatio = {600.0f/800.0f},
                .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
                //CLAY_AUTO_ID({ .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
            }
        }
    }
}

void InGameLayout(){
}

Clay_RenderCommandArray CreateLayout(bool mobileScreen, float lerpValue) {
    Clay_BeginLayout();
    switch (current_page)
    {
        case PAGE_MAIN: MainLayout(); break;
        case PAGE_CHARACTERS: ChampionsLayout(); break;
        case PAGE_CHARACTER_DRAFT: ChampionDraftLayout(); break;
        case PAGE_IN_GAME: InGameLayout(); break;
    }
    
    CLAY(CLAY_ID("OuterScrollContainer"), {
        .layout = { .sizing = { CLAY_SIZING_FIXED(0), CLAY_SIZING_GROW(0) }, .layoutDirection = CLAY_LEFT_TO_RIGHT },
        .clip = { .vertical = true, .childOffset = Clay_GetScrollOffset() },
        .border = { .width = { .betweenChildren = 2 }, .color = COLOR_RED }
    });
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
