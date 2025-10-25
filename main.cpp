#define CLAY_IMPLEMENTATION
#include "clay.h"
#define length(arr) (sizeof(arr) / sizeof((arr)[0]))
float windowWidth = 1024, windowHeight = 768;
float modelPageOneZRotation = 0;
uint32_t ACTIVE_RENDERER_INDEX = 0;

const uint16_t FONT_ID_BARTLE = 0;
const uint16_t FONT_ID_BOGLE = 1;
const uint16_t FONT_ID_ROBOTO = 2;

#define DEFAULT_SPACING 16
#define DEFAULT_CORNER 10

const Clay_Color COLOR_GRAY = (Clay_Color) {50, 50, 50, 255};
const Clay_Color COLOR_TRANSPERENT = (Clay_Color) {0, 0, 0, 0};
const Clay_Color COLOR_BLACK = (Clay_Color) {0, 0, 0, 255};
const Clay_Color COLOR_LIGHT = (Clay_Color) {244, 235, 230, 255};
const Clay_Color COLOR_LIGHT_GLOW = (Clay_Color) {244, 235, 230, 45};
const Clay_Color COLOR_LIGHT_HOVER = (Clay_Color) {200, 180, 180, 255};
const Clay_Color COLOR_RED = (Clay_Color) {209, 52, 52, 255};
const Clay_Color COLOR_RED_HOVER = (Clay_Color) {125, 35, 35, 255};
const Clay_Color COLOR_BLUE = (Clay_Color) {20, 56, 115, 230};
const Clay_Color COLOR_YELLOW = (Clay_Color) {224, 176, 0, 255};

const Clay_LayoutConfig defaultLayoutConfig = (Clay_LayoutConfig) { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) };

Clay_TextElementConfig titleTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 42, .textColor = COLOR_RED };
Clay_TextElementConfig sideBarTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = COLOR_LIGHT };
Clay_TextElementConfig headerTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 36, .textColor = COLOR_LIGHT };
Clay_TextElementConfig smallHeaderTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BARTLE, .fontSize = 30, .textColor = COLOR_LIGHT, .textAlignment = CLAY_TEXT_ALIGN_CENTER };
Clay_TextElementConfig defaultTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_BOGLE, .fontSize = 28, .textColor = COLOR_LIGHT };
Clay_TextElementConfig smallTextConfig = (Clay_TextElementConfig) { .fontId = FONT_ID_ROBOTO, .fontSize = 24, .textColor = COLOR_LIGHT_HOVER};

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

uint32_t rng_state = 0x12345678u; // seed (can be any non-zero value)

unsigned int rand() {
    uint32_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng_state = x;
    return x;
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
Clay_String background_image = CLAY_STRING("images/background.jpg");
Clay_String ingame_image = CLAY_STRING("images/ingame_background.jpg");
Clay_String mainpage_image = CLAY_STRING("images/mainpage.png");
Clay_String minimap_image = CLAY_STRING("images/minimap.png");
Clay_String zander_image = CLAY_STRING("images/Zander.png");
Clay_String mainpage_wallpapers[] = {
    CLAY_STRING("images/action_wallpaper.jpg"),
    CLAY_STRING("images/action2_wallpaper.jpg"),
    CLAY_STRING("images/raigon_wallpaper.jpg"),
    CLAY_STRING("images/raigon2_wallpaper.jpg"),
    CLAY_STRING("images/sirius_wallpaper.jpg"),
    CLAY_STRING("images/freya_wallpaper.jpg"),
    CLAY_STRING("images/ezio_wallpaper.jpg"),
    CLAY_STRING("images/alysia_wallpaper.jpg"),
};
uint8_t mainpage_index;

typedef struct{
    Clay_String name;
    Clay_String icon;
    Clay_String image;
    Clay_String ability[4];
    Clay_String ability_name[4];
    Clay_String ability_info[4];
    Clay_String lore;
    Clay_String type;
    Clay_String date;
}CharInfo;

typedef enum :int8_t{
    CHARACTER_NONE = -1,
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
    CHARACTER_LAST,
}Characters;

typedef enum :int8_t{
    CHARACTER_FILTER_ALL,
    CHARACTER_FILTER_ASSIASINS,
    CHARACTER_FILTER_FIGHTERS,
    CHARACTER_FILTER_TANKS,
    CHARACTER_FILTER_MARKSMEN,
    CHARACTER_FILTER_MAGES,
    CHARACTER_FILTER_SUPPORTS,
}CharacterFilter;

CharInfo characters[CHARACTER_LAST] = {
    {//0
        .name = CLAY_STRING("Alysia"),
        .icon = CLAY_STRING("images/alysia_icon.jpg"),
        .image = CLAY_STRING("images/Alysia.png"),
        .ability = {
            CLAY_STRING("images/alysia_ability1.png"), 
            CLAY_STRING("images/alysia_ability2.png"), 
            CLAY_STRING("images/alysia_ability3.png"), 
            CLAY_STRING("images/alysia_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Frost Bolt"),
            CLAY_STRING("Ice Lance"),
            CLAY_STRING("Arctic Wind"),
            CLAY_STRING("Frozen Gallery"),
        },
        .ability_info = {
            CLAY_STRING("Launch a cold bolt that deals damage. Deals bonus damage and adds Chill duration to enemies affected by Chill ."),
            CLAY_STRING("Projectile attack that deals damage and inflicts Chill . Deals bonus damage to enemies already affected by Chill ."),
            CLAY_STRING("Soar gracefully to target location ."), 
            CLAY_STRING("Mark a path of frost in front of you. After a delay, the patch deals damage and inflicts Freeze to enemies hit, turning them into beautiful statues ."),
        },
        .lore = CLAY_STRING("From the frozen north comes the ice sculptor Alysia. Her power allows her to wield ice with both grace and deadly precision. She can damage and freeze her enemies from a distance, while shielding her allies with ice. Obsessed with shapes and form, she left her icy fortress of solitude to seek inspiration in the arena. "),
        .type = CLAY_STRING("Mage"),
        .date = CLAY_STRING("13/12/2017")
    },
    {//1
        .name = CLAY_STRING("Ashka"),
        .icon = CLAY_STRING("images/ashka_icon.jpg"),
        .image = CLAY_STRING("images/Ashka.png"),
        .ability = {
            CLAY_STRING("images/ashka_ability1.png"), 
            CLAY_STRING("images/ashka_ability2.png"), 
            CLAY_STRING("images/ashka_ability3.png"), 
            CLAY_STRING("images/ashka_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Fireball"),
            CLAY_STRING("Fire Storm"),
            CLAY_STRING("Searing Flight"),
            CLAY_STRING("Infernal Scorch"),
        },
        .ability_info = {
            CLAY_STRING("Launch a Fireball that deals damage. Reapplies Ignite on impact ."),
            CLAY_STRING("Launch 3 Fire Storm projectiles . Each projectile deals damage and Ignites the enemy dealing damage over time ."),
            CLAY_STRING("Transform into fire and fly to target location dealing damage to nearby enemies ."), 
            CLAY_STRING("Transform into a fire elemental and dash forward dealing damage and Igniting enemies . Scorches the ground below you dealing damage while standing in it ."),
        },
        .lore = CLAY_STRING("A masked creature specialized in dark sorcery and pyro-kinetic powers. Casting devastating fire spells on his opponents, staying away from close combat. Don’t be fooled by his size, Ashka’s inner demon strikes fear into all who face him. "),
        .type = CLAY_STRING("Mage"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//2
        .name = CLAY_STRING("Bakko"),
        .icon = CLAY_STRING("images/bakko_icon.jpg"),
        .image = CLAY_STRING("images/Bakko.png"),
        .ability = {
            CLAY_STRING("images/bakko_ability1.png"), 
            CLAY_STRING("images/bakko_ability2.png"), 
            CLAY_STRING("images/bakko_ability3.png"), 
            CLAY_STRING("images/bakko_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("War Axe"),
            CLAY_STRING("Blood Axe"),
            CLAY_STRING("Valiant Leap"),
            CLAY_STRING("Heroic Charge"),
        },
        .ability_info = {
            CLAY_STRING("Swing your axe to deal damage . Each charge increases the power of Blood Axe ."),
            CLAY_STRING("Throw an axe that deals damage . Deals bonus damage for each weapon charge ."),
            CLAY_STRING("Leap into the air and strike down upon your enemies dealing damage and inflicting Snare ."), 
            CLAY_STRING("Rush forward and grab an enemy dealing damage and pushing it in front of you . Deals area damage and inflicts Stun when reaching max distance or when colliding with a wall or another enemy ."),
        },
        .lore = CLAY_STRING("Bakko has a proud history, filled with bravery and courage. Most known for saving hundreds of people from getting slaughtered by the giants in the north. A heroic brawler armed with axe and shield. Bakko uses his shield to outmaneuver opponents and protect his teammates. Watch out for his earth shattering dash attacks, even calm warriors sometimes go berserk. "),
        .type = CLAY_STRING("Tank"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//3
        .name = CLAY_STRING("Blossom"),
        .icon = CLAY_STRING("images/blossom_icon.jpg"),
        .image = CLAY_STRING("images/Blossom.png"),
        .ability = {
            CLAY_STRING("images/blossom_ability1.png"), 
            CLAY_STRING("images/blossom_ability2.png"), 
            CLAY_STRING("images/blossom_ability3.png"), 
            CLAY_STRING("images/blossom_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Thwack!"),
            CLAY_STRING("Nourish"),
            CLAY_STRING("Hop"),
            CLAY_STRING("Dance of the Dryads"),
        },
        .ability_info = {
            CLAY_STRING("Throw an infused acorn that deals damage . Deals extra damage and inflicts Snare if weapon is fully charged ."),
            CLAY_STRING("Summon the vitalizing powers of nature to heal nearest ally. Applies Butterflies, healing ally over time."),
            CLAY_STRING("Hop towards target location and avoid incoming attacks . Grants invisibility , increased movement speed and removes movement impairing effects upon landing . Invisibility fades when an ability is used ."), 
            CLAY_STRING("Launch into the air and unleash 3 waves of energy upon landing. Each wave deals damage and inflicts Weaken to enemies struck ."),
        },
        .lore = CLAY_STRING("Blossom is a happy, bubbly young faun from the Silverdeep Forest. She has left her home to investigate a disturbance in the natural order. She is always accompanied by her bird companion, Maxwell. "),
        .type = CLAY_STRING("Support"),
        .date = CLAY_STRING("27/6/2017")
    },
    {//4
        .name = CLAY_STRING("Croak"),
        .icon = CLAY_STRING("images/croak_icon.jpg"),
        .image = CLAY_STRING("images/Croak.png"),
        .ability = {
            CLAY_STRING("images/croak_ability1.png"), 
            CLAY_STRING("images/croak_ability2.png"), 
            CLAY_STRING("images/croak_ability3.png"), 
            CLAY_STRING("images/croak_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Blade Flurry"),
            CLAY_STRING("Toxin Muck"),
            CLAY_STRING("Frog Leap"),
            CLAY_STRING("Venom Wind"),
        },
        .ability_info = {
            CLAY_STRING("Strike with your blades to deal damage . Frog Leap and Camouflage increase the attack speed for a limited amount of strikes ."),
            CLAY_STRING("Spit toxin muck at target location . Deals damage , inflicts Toxin healing you over time ."),
            CLAY_STRING("Leap attack and strike with your blades to deal damage in front of you . Can be recast ."), 
            CLAY_STRING("Dash forward in the shape of a venomous wind , piercing through enemies inflicting Venom , dealing massive damage after a duration ."),
        },        
        .lore = CLAY_STRING("His background is shrouded in mystery, his movements are supernatural and his reputation is whispered about. Croak loves to surprise his enemies, using his mobility to move in and out. His chameleonic stealth and flexible fighting style makes him exceptionally hard to catch. "),
        .type = CLAY_STRING("Assasin"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//5
        .name = CLAY_STRING("Destiny"),
        .icon = CLAY_STRING("images/destiny_icon.jpg"),
        .image = CLAY_STRING("images/Destiny.png"),
        .ability = {
            CLAY_STRING("images/destiny_ability1.png"), 
            CLAY_STRING("images/destiny_ability2.png"), 
            CLAY_STRING("images/destiny_ability3.png"), 
            CLAY_STRING("images/destiny_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Power Blaster"),
            CLAY_STRING("Charged Bolt"),
            CLAY_STRING("Magnetic Orb"),
            CLAY_STRING("Pinball"),
        },
        .ability_info = {
            CLAY_STRING("Projectile attack that deals damage . Successful hits reduces the cooldown of Charged Bolt ."),
            CLAY_STRING("Hold to charge a projectile to increase damage and distance over time . The projectile deals damage and inflicts Spell Block ."),
            CLAY_STRING("Compress yourself into an orb to dispel movement impairing effects and increase movement speed . Deals damage to the first enemy you hit and knocks them back ."), 
            CLAY_STRING("Compress yourself into an orb and dash forward at supersonic speed . Deals damage ande inflicts Stun to enemies hit . Bounces off walls up to a total of 3 times ."),
        },
        .lore = CLAY_STRING("An elite sky ranger from the secluded, invisible city of Enza. Many years of rigorous combat training using Magi-tech weaponry makes Destiny an agile and deadly force to be reckoned with. Fed up with the strict regulations of living in Enza, Destiny often escapes the city to blow off some steam and have fun in the arena. "),
        .type = CLAY_STRING("Marksman"),
        .date = CLAY_STRING("22/11/2017")
    },
    {//6
        .name = CLAY_STRING("Ezmo"),
        .icon = CLAY_STRING("images/ezmo_icon.jpg"),
        .image = CLAY_STRING("images/Ezmo.png"),
        .ability = {
            CLAY_STRING("images/ezmo_ability1.png"), 
            CLAY_STRING("images/ezmo_ability2.png"), 
            CLAY_STRING("images/ezmo_ability3.png"), 
            CLAY_STRING("images/ezmo_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Arcane Fire"),
            CLAY_STRING("Chaos Grip"),
            CLAY_STRING("Displace"),
            CLAY_STRING("Grimoire of Chaos"),
        },
        .ability_info = {
            CLAY_STRING("Launch a bolt of arcane fire that deals damage."),
            CLAY_STRING("Hold to charge a projectile increasing damage and distance the longer it's charged. The projectile deals damage, knocks nearby enemies back and pulls enemies far away towards you."),
            CLAY_STRING("Turn into arcane energy, travel to target location and recharge Arcane Fire. Can be recasted."), 
            CLAY_STRING("Throw the Grimoire of Chaos to target location. The power of the grimoire draws nearby enemies towards it. It explodes after a duration dealing massive damage."),
        },
        .lore = CLAY_STRING("Ezmo was once imprisoned by the warlock, Aradu The Reserved, but managed to escape when his captor was too engrossed in reading his tome. Ezmo sealed Aradu's soul within the book, creating the Lost Soul Grimoire, and has carried it ever since. When he isn't playing tricks on people, Ezmo is searching for a way back to his home dimension. "),
        .type = CLAY_STRING("Mage"),
        .date = CLAY_STRING("11/10/2016")
    },
    {//7
        .name = CLAY_STRING("Freya"),
        .icon = CLAY_STRING("images/freya_icon.jpg"),
        .image = CLAY_STRING("images/Freya.png"),
        .ability = {
            CLAY_STRING("images/freya_ability1.png"), 
            CLAY_STRING("images/freya_ability2.png"), 
            CLAY_STRING("images/freya_ability3.png"), 
            CLAY_STRING("images/freya_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Bash"),
            CLAY_STRING("Storm Mace"),
            CLAY_STRING("Spring"),
            CLAY_STRING("Lightning Strike"),
        },
        .ability_info = {
            CLAY_STRING("Strike with your maces to deal damage. Deals bonus damage if target is affected by Static."),
            CLAY_STRING("Projectile attack that deals damage and inflicts Static, enemy is knocked back if already affected by Static. Knocking an enemy into a wall Incapacitates it."),
            CLAY_STRING("Leap towards target location and gain increased movement speed. Your next Bash deals bonus damage."), 
            CLAY_STRING("Leap high into the air striking down upon your enemies. Deals damage and inflicts Static. Deals bonus damage if the enemy is already affected by Static."),
        },
        .lore = CLAY_STRING("Once a tribe queen, now a fearless contender. Her titanic hammers and overwhelming power of lighting is the perfect recipe for destruction, slowing down her foes with thundering spells to catch them off guard."),
        .type = CLAY_STRING("Fighter"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//8
        .name = CLAY_STRING("Iva"),
        .icon = CLAY_STRING("images/iva_icon.jpg"),
        .image = CLAY_STRING("images/Iva.png"),
        .ability = {
            CLAY_STRING("images/iva_ability1.png"), 
            CLAY_STRING("images/iva_ability2.png"), 
            CLAY_STRING("images/iva_ability3.png"), 
            CLAY_STRING("images/iva_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Boomstick"),
            CLAY_STRING("ROCKET X-67"),
            CLAY_STRING("Jet Pack"),
            CLAY_STRING("Machine Gun"),
        },
        .ability_info = {
            CLAY_STRING("Fire 4 bullets in a cone, each dealing damage."),
            CLAY_STRING("Launch a rocket that deals damage on impact and area damage. The explosion Ignites enemies affected by Oil, taking damage over time."),
            CLAY_STRING("Fire up your Jet Pack and travel towards target location. Inflicts Oil on enemies below you."), 
            CLAY_STRING("Fire a series of piercing projectiles dealing damage."),
        },
        .lore = CLAY_STRING("A scavenger from the outer realms. Iva has engineered her own arsenal of weapons. Firing crazy rockets or unleashing a storm of bullets is her way of greeting her opponents in the Arena."),
        .type = CLAY_STRING("Marksman"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//9
        .name = CLAY_STRING("Jade"),
        .icon = CLAY_STRING("images/jade_icon.jpg"),
        .image = CLAY_STRING("images/Jade.png"),
        .ability = {
            CLAY_STRING("images/jade_ability1.png"), 
            CLAY_STRING("images/jade_ability2.png"), 
            CLAY_STRING("images/jade_ability3.png"), 
            CLAY_STRING("images/jade_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Revolver Shot"),
            CLAY_STRING("Snipe"),
            CLAY_STRING("Blast Vault"),
            CLAY_STRING("Explosive Shells"),
        },
        .ability_info = {
            CLAY_STRING("Fire a revolver shot dealing damage."),
            CLAY_STRING("Fire a piercing bullet that deals damage and inflicts Stun."),
            CLAY_STRING("Detonate a grenade that launches you into the air. The explosion deals damage and inflicts Stun on nearby enemies."), 
            CLAY_STRING("Fire a series of explosive shells dealing damage and area damage around the impact."),
        },
        .lore = CLAY_STRING("A mysterious gunslinger with a score to settle. Born with eagle-eyes and armed with a lethal sniper rifle, Jade’s pinpoint accuracy is a serious threat for anyone who enters the arena. For close encounters she prefers a good old combination of stealth and homemade revolvers. The rumor says Jade joined the arena games to find the villain who killed her brother. Will she complete her dark quest of vengeance?"),
        .type = CLAY_STRING("Marksman"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//10
        .name = CLAY_STRING("Jamila"),
        .icon = CLAY_STRING("images/jamila_icon.jpg"),
        .image = CLAY_STRING("images/Jamila.png"),
        .ability = {
            CLAY_STRING("images/jamila_ability1.png"), 
            CLAY_STRING("images/jamila_ability2.png"), 
            CLAY_STRING("images/jamila_ability3.png"), 
            CLAY_STRING("images/jamila_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Assassin's Cut"),
            CLAY_STRING("Shuriken"),
            CLAY_STRING("Elusive Strike"),
            CLAY_STRING("Stalking Phantom"),
        },
        .ability_info = {
            CLAY_STRING("Three quick stabs that each deal damage, followed by a heavy strike."),
            CLAY_STRING("Throw a bouncing shuriken, dealing damage and inflicting Snare."),
            CLAY_STRING("Hold to charge a dash to increase damage and range. It deals damage and dashing in to a wall triggers a wall jump that inflicts Incapacitate."), 
            CLAY_STRING("Summon a phantom that dashes forward, dealing damage and stopping behind the first enemy hit. Upon arrival, it chases and strikes enemies in bursts."),
        },
        .lore = CLAY_STRING("A young assassin from the Shadowblade clan, Jamila suddenly found herself the new leader after the death of her mother, the previous matriarch. As the youngest leader in the clan’s history, some call her too inexperienced and undeserving of the title. Determined to silence those detractors, Jamila enters the arena to prove her mastery of the Shadow Arts. "),
        .type = CLAY_STRING("Assasin"),
        .date = CLAY_STRING("5/3/2018")
    },
    {//11
        .name = CLAY_STRING("Jumong"),
        .icon = CLAY_STRING("images/jumong_icon.jpg"),
        .image = CLAY_STRING("images/Jumong.png"),
        .ability = {
            CLAY_STRING("images/jumong_ability1.png"), 
            CLAY_STRING("images/jumong_ability2.png"), 
            CLAY_STRING("images/jumong_ability3.png"), 
            CLAY_STRING("images/jumong_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Hunting Arrow"),
            CLAY_STRING("Steady Shot"),
            CLAY_STRING("Black Arrow"),
            CLAY_STRING("Dragon Slayer"),
        },
        .ability_info = {
            CLAY_STRING("Fire an arrow that deals damage. Successful hits charges your bow. A fully charged bow enables you to recast Steady Shot or Black Arrow."),
            CLAY_STRING("Projectile attack that deals damage."),
            CLAY_STRING("Dash towards move direction and fire an arrow that deals damage."), 
            CLAY_STRING("Hold to charge an arrow to increase its damage and travel distance. The arrow deals damage and pulls the target hit with it. Pulling a target into an enemy or into a wall deals damage to the enemy hit and stuns both targets."),
        },
        .lore = CLAY_STRING("Jumong is a trophy collector who has wandered the wildlands in the pursuit of a worthy challenge. No longer content with hunting the great beasts of the world, has led him to enter the arena looking for a new type of prey. He traps his foes and ends them with a well placed shot from his mighty bow."),
        .type = CLAY_STRING("Marksman"),
        .date = CLAY_STRING("21/10/2016")
        
    },
    {//12
        .name = CLAY_STRING("Lucie"),
        .icon = CLAY_STRING("images/lucie_icon.jpg"),
        .image = CLAY_STRING("images/Lucie.png"),
        .ability = {
            CLAY_STRING("images/lucie_ability1.png"), 
            CLAY_STRING("images/lucie_ability2.png"), 
            CLAY_STRING("images/lucie_ability3.png"), 
            CLAY_STRING("images/lucie_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Toxic Bolt"),
            CLAY_STRING("Healing Potion"),
            CLAY_STRING("Barrier"),
            CLAY_STRING("Crippling Goo"),
        },
        .ability_info = {
            CLAY_STRING("Projectile attack that deals damage and inflicts Toxic."),
            CLAY_STRING("Throw a potion that heals the nearest ally for and applies Revitalize, healing Recharges faster for each nearby ally."),
            CLAY_STRING("Shield a target ally, absorbing damage."), 
            CLAY_STRING("Throw a vial of mixed chemicals to target location dealing impact damage and Snaring enemies. Covers the ground in a crippling goo that deals damage over a short duration."),
        },
        .lore = CLAY_STRING("Lucie is a highly skilled Alchemist. A rebel at school who got expelled from the Toleen Academy for mixing banned potions. She is a diverse contender who knows which brew makes you choke, heal or flee in fear. The arena has become her new playground for wild experiments. Seeing her smile while mixing ingredients of an unknown nature might make her look more crazy than cute."),
        .type = CLAY_STRING("Support"),
        .date = CLAY_STRING("8/11/2017")
    },
    {//13
        .name = CLAY_STRING("Raigon"),
        .icon = CLAY_STRING("images/raigon_icon.jpg"),
        .image = CLAY_STRING("images/Raigon.png"),
        .ability = {
            CLAY_STRING("images/raigon_ability1.png"), 
            CLAY_STRING("images/raigon_ability2.png"), 
            CLAY_STRING("images/raigon_ability3.png"), 
            CLAY_STRING("images/raigon_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Sword Slash"),
            CLAY_STRING("Retribution"),
            CLAY_STRING("Heavenly Strike"),
            CLAY_STRING("Wrath of the Tiger"),
        },
        .ability_info = {
            CLAY_STRING("Strike with your sword to deal damage. Successful hits charges your weapon and reduces the cooldown of Retribution. Each charge increases the power of Retribution."),
            CLAY_STRING("Dash forward and strike to deal damage and leech health. Deals bonus damage and leeches bonus health per weapon charge."),
            CLAY_STRING("Leap and strike with your sword to deal damage in front of you upon landing."), 
            CLAY_STRING("Dash and strike an enemy, dealing damage. Upon hit, slash all enemies in an area around you, dealing damage over time. When the duration ends or upon recast, dash towards your aim direction."),
        },
        .lore = CLAY_STRING("Raigon is the former crown prince of Quna. He was a well-respected figure among the kingdom's warriors, until he was exiled due to the manipulations of Pestilus. For now, the arena serves as a decent place to find work and set the wheels in motion for his retaliation."),
        .type = CLAY_STRING("Fighter"),
        .date = CLAY_STRING("15/2/2017")
    },
    {//14
        .name = CLAY_STRING("Sirius"),
        .icon = CLAY_STRING("images/sirius_icon.jpg"),
        .image = CLAY_STRING("images/Sirius.png"),
        .ability = {
            CLAY_STRING("images/sirius_ability1.png"), 
            CLAY_STRING("images/sirius_ability2.png"), 
            CLAY_STRING("images/sirius_ability3.png"), 
            CLAY_STRING("images/sirius_ability4.png"),
        },
        .ability_name = {
            CLAY_STRING("Crescent Strike"),
            CLAY_STRING("Sunlight"),
            CLAY_STRING("Celestial Split"),
            CLAY_STRING("Astral Beam"),
        },
        .ability_info = {
            CLAY_STRING("Strike to deal damage to an enemy. Deals damage and inflicts weaken if weapon is fully charged. Your weapon charges over time."),
            CLAY_STRING("Call down a beam of sunlight that heals the nearest ally. Recharges faster for each nearby ally."),
            CLAY_STRING("Teleport to the target location dealing damage to nearby enemies and healing nearby allies."), 
            CLAY_STRING("Channel a beam of light that deals damage to enemies, heals allies, and heals self for."),
        },
        .lore = CLAY_STRING("Born under a purple sky and wrapped in the light of the brightest star, the prophecy was true and foretold the birth of the Zenith. Taken from his parents and sent to a monastery to learn the the way of the astronomers, Sirius is a calm prodigy who uses the energies of stars and planets as destructive spells and healing powers."),
        .type = CLAY_STRING("Support"),
        .date = CLAY_STRING("8/11/2017")
    },
};

Characters characterInfoSelect = CHARACTER_NONE;
Characters characterDraftSelect = CHARACTER_NONE;

void HeaderBar(){
    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = {CLAY_SIZING_GROW(0) , CLAY_SIZING_FIT(0) }, . childGap = DEFAULT_SPACING}}){
        CLAY_TEXT(CLAY_STRING("League of the Ancients"), &titleTextConfig);
        CLAY(CLAY_ID("Spacer"), { .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
        CLAY_TEXT(CLAY_STRING("Shop"), &sideBarTextConfig);
        }
                CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            CLAY_TEXT(CLAY_STRING("Profile"), &sideBarTextConfig);
        }
    }
}
void SideBar(){
    sideBarTextConfig.userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true });
    CLAY(CLAY_ID("SideBar"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, 
    .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER }, .padding = { 0, DEFAULT_SPACING, 0, 0, }, .childGap = DEFAULT_SPACING }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT } }) {
        CLAY(CLAY_ID("Spacer_TOP"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_MAIN? COLOR_RED_HOVER : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased && current_page != PAGE_MAIN){
                mainpage_index = rand() % length(mainpage_wallpapers);
                current_page = PAGE_MAIN;
            } 
            CLAY_TEXT(CLAY_STRING("Home"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_CHARACTERS ? COLOR_RED_HOVER : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_CHARACTERS;
            CLAY_TEXT(CLAY_STRING("Characters"), &sideBarTextConfig);
        }
        CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_CHARACTER_DRAFT ? COLOR_RED_HOVER : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)
        }) {
            if(Clay_Hovered() && input.isMouseReleased) current_page = PAGE_CHARACTER_DRAFT;
            CLAY_TEXT(CLAY_STRING("Play"), &sideBarTextConfig);
        }
        CLAY(CLAY_ID("Spacer_BOT"), { .layout = { .sizing = { .height = CLAY_SIZING_GROW(0) } } });
    }
}

void MainLayout(){
    CLAY(CLAY_ID("ContentContainer"), { .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
        CLAY(CLAY_ID("MISSIONS"), { .backgroundColor = COLOR_BLUE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIXED(500), CLAY_SIZING_FIXED(400) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }, 
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_LEFT_BOTTOM, CLAY_ATTACH_POINT_LEFT_BOTTOM}}}){
            CLAY_TEXT(CLAY_STRING("Missions"), &smallHeaderTextConfig);
            CLAY_AUTO_ID({.border = { .width = {0, 0, 2, 2}, .color = COLOR_LIGHT },.cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
            .layout  = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}});
            CLAY_AUTO_ID({.layout = defaultLayoutConfig}) {
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
        CLAY(CLAY_ID("News"), { .backgroundColor = COLOR_BLUE, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER), .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIXED(400), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }, 
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_RIGHT_TOP, CLAY_ATTACH_POINT_RIGHT_TOP}}}){
            CLAY_TEXT(CLAY_STRING("News"), &smallHeaderTextConfig);
            CLAY_AUTO_ID({.border = { .width = {0, 0, 2, 2}, .color = COLOR_LIGHT },.cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
            .layout  = { .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0)}}});
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { .width = CLAY_SIZING_GROW(0) }}}){
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(0) }, .padding = {0,0,0,10}}}){
                    CLAY_TEXT(CLAY_STRING(" Zander Comes to the arena: March 2026!"), &defaultTextConfig);
                }
                CLAY_AUTO_ID({.layout = {.sizing = { .width = CLAY_SIZING_GROW(0) }}, .image  = { &zander_image }, . aspectRatio = 16.0f/9.0f});

            }
            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } }, .border = { .width = {0, 0, 0, 2}, .color = COLOR_LIGHT }});
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { .width = CLAY_SIZING_GROW(0) }, .childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_CENTER}}}){
                CLAY_TEXT(CLAY_STRING("Take the Survey:     "), &defaultTextConfig);
                CLAY_AUTO_ID({
                .layout = { .padding = {8, 8, 3, 3} },
                .backgroundColor = Clay_Hovered()? COLOR_RED : current_page == PAGE_CHARACTERS ? COLOR_RED_HOVER : COLOR_BLUE,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
                .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}) {
                    if(Clay_Hovered() && input.isMouseReleased) {}//current_page = PAGE_CHARACTERS;
                    CLAY_TEXT(CLAY_STRING("Survey"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 28, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
            }

        }
    }
}

CharacterFilter characterInfoFilter;
Characters infoFillterChar[CHARACTER_LAST]{     
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
    CHARACTER_SIRIUS
};
uint32_t infoFillterNum = 15;
const char* characterRoleStrings[] = { "All", "Assasin", "Fighter", "Tank", "Marksman", "Mage", "Support"};
void UpdateFillter(CharacterFilter fillter, Characters* fillterArray, uint32_t* num){
    if(fillter == CHARACTER_FILTER_ALL){
        for(int i = 0; i < CHARACTER_LAST; i++){
            fillterArray[i] = (Characters)i;
        }
        *num = 15;
    }
    else{
        *num = 0;
        for(int i = 0; i < CHARACTER_LAST; i++){
            fillterArray[i] = CHARACTER_NONE;
        }
        for(int i = 0; i < CHARACTER_LAST; i++){
            if(characters[i].type.chars == characterRoleStrings[fillter]){
                fillterArray[*num] = (Characters)i;
                *num+=1;
            }
        }
    }
}

void CharactersContainer(){
    CLAY(CLAY_ID("CharactersContainer"), { .layout =  { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }}){
        CLAY(CLAY_ID("Fillters"),{.layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}, .childGap = DEFAULT_SPACING}}){
            CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_PERCENT(0.15f), CLAY_SIZING_FIT(0) }, .padding = {16, 16, 6, 6} }, .backgroundColor = COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
            }) {
                CLAY_TEXT(CLAY_STRING(" Search"), &smallTextConfig);
            }
            CLAY_AUTO_ID({.layout =  {.sizing = { CLAY_SIZING_GROW(0)}}});
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_ALL;
                    UpdateFillter(characterInfoFilter, infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("ALL"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_ALL? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_ASSIASINS;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("ASSASINS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_ASSIASINS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_FIGHTERS;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("FIGHTERS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_FIGHTERS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_TANKS;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("TANKS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_TANKS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_MARKSMEN;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("MARKSMEN"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_MARKSMEN? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_MAGES;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("MAGES"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_MAGES? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
            CLAY_AUTO_ID({.layout = { .padding = {0, DEFAULT_SPACING, 2, 2} }}) {
                bool hovered = Clay_Hovered();
                if( hovered && input.isMouseReleased){
                    characterInfoFilter = CHARACTER_FILTER_SUPPORTS;
                    UpdateFillter(characterInfoFilter,infoFillterChar, &infoFillterNum);
                } 
                CLAY_TEXT(CLAY_STRING("SUPPORTS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = hovered? COLOR_RED : characterInfoFilter == CHARACTER_FILTER_SUPPORTS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
        }
        CLAY(CLAY_ID("CharactersContainer2"), { .layout =  { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }}){
            int collumns = (windowWidth*0.88f)/240;
            int rows = infoFillterNum/collumns;
            for(int i = 0; i < rows; i++){
                CLAY_AUTO_ID({ .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING}}){
                    for(int y = i*collumns; y < collumns * (i+1); y++){
                        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }}}){
                            bool hovered = Clay_Hovered();
                            if(hovered && input.isMouseReleased){
                                characterInfoSelect = infoFillterChar[y];
                            }
                            CLAY_AUTO_ID({ .image = { .imageData = &characters[infoFillterChar[y]].icon},
                            .layout = {.sizing = { CLAY_SIZING_FIXED(220),CLAY_SIZING_FIXED(120)}},
                            .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(characters[infoFillterChar[y]].name, CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                                .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            }
                        }
                    }
                }
            }
            CLAY_AUTO_ID({ .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING}}){
                for(int i = collumns*rows; i < infoFillterNum; i++){
                    CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                            bool hovered = Clay_Hovered();
                            if(hovered && input.isMouseReleased){
                                characterInfoSelect = infoFillterChar[i];
                            }
                            CLAY_AUTO_ID({ .image = { .imageData = &characters[infoFillterChar[i]].icon},
                            .layout = {.sizing = { CLAY_SIZING_FIXED(220),CLAY_SIZING_FIXED(120)}},
                            .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : COLOR_LIGHT}});
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(characters[infoFillterChar[i]].name, CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                                .textColor =  hovered? COLOR_RED : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            }
                        }
                }
            }
        }
    }
}


void CharacterInfo(CharInfo& character){
    CLAY(CLAY_ID("CharContainer"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }}){
        CLAY(CLAY_ID("Name"), { 
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}, 
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_RIGHT_TOP, CLAY_ATTACH_POINT_CENTER_TOP}}}){
            CLAY_TEXT(character.name, &headerTextConfig);
        }
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }},
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP}}}){
            CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() ? COLOR_RED : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}) {
                if(Clay_Hovered() && input.isMouseReleased) 
                {
                    characterInfoSelect = CHARACTER_NONE;
                }
                CLAY_TEXT(CLAY_STRING("Back"), &sideBarTextConfig);
            }
        }
        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_PERCENT(0.55f), CLAY_SIZING_GROW(0) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}}}){
            CLAY(CLAY_ID("IMAGE"), { .image = { .imageData = &character.image}, .aspectRatio = {670.0f/666.0f},
            .layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
        }
        CLAY_AUTO_ID({.clip{false, true}, .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, .padding = {0, DEFAULT_SPACING*2, 40, 0}, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP}}}){
            CLAY_TEXT(CLAY_STRING("      Lore"), &sideBarTextConfig);
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25), .backgroundColor = COLOR_BLUE}){
                CLAY_TEXT(character.lore, &smallTextConfig);
            }
            CLAY_TEXT(CLAY_STRING("      Abilites"), &sideBarTextConfig);
            for(int i = 0; i < 4; i++){
                CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {DEFAULT_SPACING, DEFAULT_SPACING, 8, 8}, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}},
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT }, .cornerRadius = CLAY_CORNER_RADIUS(25), .backgroundColor = COLOR_BLUE}){
                    CLAY_AUTO_ID({ .image = { .imageData = &character.ability[i]}, .layout = {.sizing = { CLAY_SIZING_FIXED(64), CLAY_SIZING_FIXED(64) }}});
                    CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP}}}){
                        CLAY_TEXT(character.ability_name[i], &sideBarTextConfig);
                        CLAY_TEXT(character.ability_info[i], &smallTextConfig);
                    }
                }
            }
            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(.max = 700), CLAY_SIZING_FIT(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_CENTER}}}){
                CLAY_TEXT(CLAY_STRING("      Role:  "), &sideBarTextConfig);
                CLAY_TEXT(character.type, &sideBarTextConfig);
                CLAY_AUTO_ID({.layout = {.sizing = { .width = CLAY_SIZING_GROW(0)}}});
                CLAY_TEXT(CLAY_STRING("Release:  "), &sideBarTextConfig);
                CLAY_TEXT(character.date, &sideBarTextConfig);
            }
        }
    }
}


void ChampionsLayout(){
    switch (characterInfoSelect)
    {
        case CHARACTER_NONE: case CHARACTER_LAST: CharactersContainer(); break;
        default: CharacterInfo(characters[characterInfoSelect]); break;
    }
}

CharacterFilter characterDraftFilter;
Characters draftFillterChar[CHARACTER_LAST]{     
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
    CHARACTER_SIRIUS
};
uint32_t draftFillterNum = 15;
bool selectGameMode = true;
typedef enum : uint8_t{
    GAME_MODE_ARENA_3V3,
    GAME_MODE_ARENA_2V2,
    GAME_MODE_CLASSIC_5V5,
}GameMode;
const Clay_String selectGameModes[3] = {CLAY_STRING("3v3\nArena"), CLAY_STRING("2v2\nArena"), CLAY_STRING("5v5\nClassic")};
const Clay_String gameModes[3] = {CLAY_STRING("Arena 3v3"), CLAY_STRING("Arena 2v2"), CLAY_STRING("Classic 5v5")};
GameMode gameMode;



void GameModeSelectLayout(){
    CLAY(CLAY_ID("WrapContainer"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }}){
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
            CLAY_AUTO_ID({
            .layout = { .padding = {16, 16, 6, 6} },
            .backgroundColor = Clay_Hovered() ? COLOR_RED : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}) {
                if(Clay_Hovered() && input.isMouseReleased) 
                {
                    selectGameMode = false;
                }
                CLAY_TEXT(CLAY_STRING("Back"), &sideBarTextConfig);
            }
        }
        for(int i = 0; i < 3; i++){
            CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, . childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}},
            .backgroundColor = Clay_Hovered()? COLOR_LIGHT_GLOW : COLOR_TRANSPERENT}){
                bool hovered = Clay_Hovered();
                if(hovered && input.isMouseReleased){
                    gameMode = (GameMode)i;
                    selectGameMode = false;
                }
                CLAY_TEXT(selectGameModes[i], CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BARTLE, .fontSize = 42, .textColor = hovered? COLOR_RED : gameMode == i? COLOR_RED_HOVER : COLOR_LIGHT, .textAlignment = CLAY_TEXT_ALIGN_CENTER, .lineHeight = 72, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
        }
    }
}


void ChampionDraftLayout(){
    CLAY(CLAY_ID("WrapContainer"), { .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING }}){
        CLAY(CLAY_ID("CharactersContainer"), { .layout =  { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING, .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP}, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING) }}){
            CLAY(CLAY_ID("Fillters"),{.layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}, .childGap = 8}}){
                CLAY_AUTO_ID({ .layout = { .sizing = { CLAY_SIZING_PERCENT(0.2f), CLAY_SIZING_FIT(0) }, .padding = {16, 24, 6, 6} }, .backgroundColor = COLOR_BLUE,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
                .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER),
                }) {
                    CLAY_TEXT(CLAY_STRING("Search"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT_HOVER}));
                }
                CLAY_AUTO_ID({.layout =  {.sizing = { CLAY_SIZING_GROW(0)}}});
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_ALL;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("ALL"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_ALL? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_ASSIASINS;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("ASSASINS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_ASSIASINS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_FIGHTERS;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("FIGHTERS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_FIGHTERS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_TANKS;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("TANKS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_TANKS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_MARKSMEN;
                        UpdateFillter(characterDraftFilter,draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("MARKSMEN"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_MARKSMEN? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }, .border = { .width = {0, 2, 0, 0}, .color = COLOR_LIGHT }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_MAGES;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("MAGES"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_MAGES? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
                CLAY_AUTO_ID({.layout = { .padding = {0, 8, 2, 2} }}) {
                    bool hovered = Clay_Hovered();
                    if( hovered && input.isMouseReleased){
                        characterDraftFilter = CHARACTER_FILTER_SUPPORTS;
                        UpdateFillter(characterDraftFilter, draftFillterChar, &draftFillterNum);
                    } 
                    CLAY_TEXT(CLAY_STRING("SUPPORTS"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = hovered? COLOR_RED : characterDraftFilter == CHARACTER_FILTER_SUPPORTS? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                }
            }
            int collumns = 4;
            int rows = draftFillterNum/collumns;
            for(int i = 0; i < rows; i++){
                CLAY_AUTO_ID({ .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
                    for(int y = i*collumns; y < collumns * (i+1); y++){
                        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                            bool hovered = Clay_Hovered();
                            if(hovered && input.isMouseReleased && draftFillterChar[y] != CHARACTER_ALYSIA && draftFillterChar[y] != CHARACTER_BAKKO){
                                characterDraftSelect = (Characters)draftFillterChar[y];
                            }
                            CLAY_AUTO_ID({ .image = { .imageData = &characters[draftFillterChar[y]].icon},
                            .layout = {.sizing = { CLAY_SIZING_FIXED(120),CLAY_SIZING_FIXED(65)}},
                            .border = { .width = { 2,2,2,2}, .color = draftFillterChar[y] == CHARACTER_ALYSIA || draftFillterChar[y] == CHARACTER_BAKKO? COLOR_GRAY : hovered? COLOR_RED : characterDraftSelect == draftFillterChar[y]? COLOR_RED_HOVER : COLOR_LIGHT}}){
                                if(draftFillterChar[y] == CHARACTER_ALYSIA || draftFillterChar[y] == CHARACTER_BAKKO){
                                    CLAY_AUTO_ID({.layout = {.sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}}, .backgroundColor = {50,50,50,200}});
                                }
                            }
                            CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                                CLAY_TEXT(characters[draftFillterChar[y]].name, CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                                .textColor =  draftFillterChar[y] == CHARACTER_ALYSIA || draftFillterChar[y] == CHARACTER_BAKKO? COLOR_GRAY : hovered? COLOR_RED : characterDraftSelect == draftFillterChar[y]? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
                                CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            }
                        }
                    }
                }
            }
            CLAY_AUTO_ID({ .layout =  { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, }}){
                for(int i = collumns*rows; i < draftFillterNum; i++){
                    CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }}}){
                        bool hovered = Clay_Hovered();
                        if(hovered && input.isMouseReleased){
                            characterDraftSelect = (Characters)draftFillterChar[i];
                        }
                        CLAY_AUTO_ID({ .image = { .imageData = &characters[draftFillterChar[i]].icon},
                        .layout = {.sizing = { CLAY_SIZING_FIXED(120),CLAY_SIZING_FIXED(65)}},
                        .border = { .width = { 2,2,2,2}, .color = hovered? COLOR_RED : characterDraftSelect == draftFillterChar[i]? COLOR_RED_HOVER : COLOR_LIGHT}});
                        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}}){
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                            CLAY_TEXT(characters[draftFillterChar[i]].name, CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, 
                            .textColor =  hovered? COLOR_RED : characterDraftSelect == draftFillterChar[i]? COLOR_RED_HOVER : COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
                            CLAY_AUTO_ID({ .layout = { .sizing = { .width = CLAY_SIZING_GROW(0) } } });
                        }
                     }
                }
            }
        }
        CLAY_AUTO_ID({.layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = 100, .padding = {0,0,0,150}}}){
            void* image = NULL;
            switch (characterDraftSelect)
            {
                case CHARACTER_NONE: case CHARACTER_LAST: case CHARACTER_ALYSIA: case CHARACTER_BAKKO: image = NULL; break;
                default: image = &characters[characterDraftSelect].image; break;
            }
            CLAY(CLAY_ID("PartMember1"), {.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_BOTTOM}}}){
                CLAY(CLAY_ID("PartMemberImage1"), { .image = { .imageData = &characters[CHARACTER_ALYSIA].image}, .aspectRatio = {670.0f/666.0f},
                .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}}}){
                    CLAY_TEXT(CLAY_STRING("Fizzy"), &smallHeaderTextConfig);
                }
            }
            CLAY(CLAY_ID("Player"), {.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}},
            .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {0, 40}}}){
                CLAY(CLAY_ID("CharImage"), { .image = { .imageData = image}, .aspectRatio = {670.0f/666.0f},
                .layout = {.sizing = {CLAY_SIZING_PERCENT(0.8f), CLAY_SIZING_PERCENT(0.8f)}, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}}}){
                    CLAY_TEXT(CLAY_STRING("You"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BARTLE, .fontSize = 30, .textColor = COLOR_YELLOW, .textAlignment = CLAY_TEXT_ALIGN_CENTER }));
                }
            }
            CLAY(CLAY_ID("PartMember2"), {.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childAlignment = {.x = CLAY_ALIGN_X_RIGHT, .y = CLAY_ALIGN_Y_BOTTOM}}}){
                if(gameMode != GAME_MODE_ARENA_2V2){
                    CLAY(CLAY_ID("PartMemberImage2"), { .image = { .imageData = &characters[CHARACTER_BAKKO].image}, .aspectRatio = {670.0f/666.0f},
                    .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)}, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}}}){
                        CLAY_TEXT(CLAY_STRING("Gorilla"), &smallHeaderTextConfig);
                    }
                }
            }
            CLAY(CLAY_ID("GameMode"), {.layout = {.sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING, .childAlignment = {.x = CLAY_ALIGN_X_LEFT, .y = !selectGameMode? CLAY_ALIGN_Y_CENTER : CLAY_ALIGN_Y_TOP}}, 
            .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_CENTER_TOP}, .offset = {0, 0}}}){
                CLAY_AUTO_ID({
                .layout = { .padding = {16, 16, 6, 6} },
                .backgroundColor = Clay_Hovered()? COLOR_RED : COLOR_BLUE,
                .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
                .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}) {
                    if(Clay_Hovered() &&input.isMouseReleased){
                        selectGameMode = true;
                    }
                    CLAY_TEXT(CLAY_STRING("Select"), &sideBarTextConfig);
                }  
                CLAY_TEXT(gameModes[gameMode], &headerTextConfig);
            }
        }
        CLAY(CLAY_ID("PLAY_BUTTON"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .childGap = DEFAULT_SPACING, .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP}},
        .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {0, 0}}
        }){
            if(characters[characterDraftSelect].type.chars != "Support")
            CLAY_TEXT(CLAY_STRING("Warrning: Support role not filled!"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_ROBOTO, .fontSize = 28, .textColor = COLOR_YELLOW}));
            CLAY_AUTO_ID({ .layout = { .padding = {32, 32, 16, 16} },
            .backgroundColor = Clay_Hovered()? COLOR_RED : COLOR_BLUE,
            .border = { .width = {2, 2, 2, 2}, .color = COLOR_LIGHT },
            .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}){
                if(Clay_Hovered() && input.isMouseReleased && characterDraftSelect != CHARACTER_NONE && characterDraftSelect != CHARACTER_LAST) current_page = PAGE_IN_GAME;
                    CLAY_TEXT(CLAY_STRING("Start"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BARTLE, .fontSize = 48, .textColor = COLOR_LIGHT, .userData =  FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true })}));
            }
        }
    }
}

void PlayMenu(){
    if(selectGameMode){
        GameModeSelectLayout();
    }
    else{
        ChampionDraftLayout();
    }
}

void InGameUI(){
    CLAY(CLAY_ID("TEAM_ICONS"),{.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .childGap = DEFAULT_SPACING,},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_LEFT_TOP, CLAY_ATTACH_POINT_LEFT_TOP}, .offset = {10, 10}}}){
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[CHARACTER_ALYSIA].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border ={.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[characterDraftSelect].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[CHARACTER_BAKKO].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
    }
    CLAY(CLAY_ID("ENEMY_ICONS"),{.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .childAlignment = {CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP}, .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .childGap = DEFAULT_SPACING,},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_RIGHT_TOP, CLAY_ATTACH_POINT_RIGHT_TOP}, .offset = {-10, 10}}}){
        CLAY_AUTO_ID({.backgroundColor = COLOR_BLUE, .cornerRadius = CLAY_CORNER_RADIUS(100), .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}})
            CLAY(CLAY_ID("MINIMAP"),{ .image = { .imageData = &minimap_image},
            .layout = {.sizing = { CLAY_SIZING_FIXED(200),CLAY_SIZING_FIXED(200)}}});
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[CHARACTER_ASHKA].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[CHARACTER_CROAK].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4}}){
            CLAY_AUTO_ID({ .image = { .imageData = &characters[CHARACTER_LUCIE].icon},
            .layout = {.sizing = { CLAY_SIZING_FIXED(80),CLAY_SIZING_FIXED(40)}},
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT}});
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER}}, 
            .backgroundColor = COLOR_RED, .border ={.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
        }
    }
    CLAY(CLAY_ID("TrayElement"),{.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM ,.sizing = { CLAY_SIZING_FIXED(600), CLAY_SIZING_FIXED(100) }, .childGap = 8, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}, .padding = {0,0,0,10}}, .backgroundColor = COLOR_GRAY, 
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {0, 0}}, .cornerRadius = {50,50,0,0}, .border = {.color = COLOR_LIGHT, .width = {2,2,2,0}}}){
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(450), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}}, 
            .backgroundColor = COLOR_RED, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("220"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(450), CLAY_SIZING_FIT(0) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}}, 
            .backgroundColor = COLOR_YELLOW, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(8)}){
                CLAY_TEXT(CLAY_STRING("100%"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
            }
    }
    CLAY(CLAY_ID("ChampIcon"),{.layout = {.sizing = { CLAY_SIZING_FIXED(160), CLAY_SIZING_FIXED(80) }}, .image = { .imageData = &characters[characterDraftSelect].icon},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {-330, -5}}, .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}});
    CLAY(CLAY_ID("Abilites"),{.layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = {CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0)}, .childGap = DEFAULT_SPACING, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING)},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_BOTTOM, CLAY_ATTACH_POINT_CENTER_BOTTOM}, .offset = {0, -64}}, .cornerRadius = CLAY_CORNER_RADIUS(DEFAULT_CORNER)}){
        CLAY_AUTO_ID({ .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = DEFAULT_SPACING*2}}){
            for(int i = 0; i < 4; i++){
                CLAY_AUTO_ID({ .image = { .imageData = &characters[characterDraftSelect].ability[i]},
                .layout = {.sizing = { CLAY_SIZING_FIXED(64),CLAY_SIZING_FIXED(64)}},
                .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT},
                .cornerRadius = CLAY_CORNER_RADIUS(32)});
            }
        }
    }
    CLAY(CLAY_ID("TopBar"),{.layout = {.sizing = { CLAY_SIZING_FIXED(420), CLAY_SIZING_FIXED(50) }, .childGap = DEFAULT_SPACING, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}, .padding = {0,0,0,10}}, .backgroundColor = COLOR_GRAY,
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_CENTER_TOP}, .offset = {0, 0}}, .cornerRadius = {0,0,50,50}, .border = {.color = COLOR_LIGHT, .width = {2,2, 0,2}}}){
        for(int i = 0; i < 3; i++){
            CLAY_AUTO_ID({ .layout = {.sizing = { CLAY_SIZING_FIXED(32),CLAY_SIZING_FIXED(32)}},
            .backgroundColor = i == 0 || i == 1? COLOR_BLUE : ColorLerp(COLOR_BLACK, COLOR_BLUE, 0.35f),
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT},
            .cornerRadius = CLAY_CORNER_RADIUS(32)});
        }
        CLAY_AUTO_ID({ .layout = {.sizing = { CLAY_SIZING_FIXED(0),CLAY_SIZING_FIXED(32)}}, .border = { .width = { 0,2,0,0}, .color = COLOR_LIGHT}});
        CLAY_TEXT(CLAY_STRING("2:21"), CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 36, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
        CLAY_AUTO_ID({ .layout = {.sizing = { CLAY_SIZING_FIXED(0),CLAY_SIZING_FIXED(32)}}, .border = { .width = { 2,0,0,0}, .color = COLOR_LIGHT}});
        for(int i = 0; i < 3; i++){
            CLAY_AUTO_ID({ .layout = {.sizing = { CLAY_SIZING_FIXED(32),CLAY_SIZING_FIXED(32)}},
            .backgroundColor = i == 2? COLOR_RED : ColorLerp(COLOR_BLACK, COLOR_RED, 0.35f),
            .border = { .width = { 2,2,2,2}, .color = COLOR_LIGHT},
            .cornerRadius = CLAY_CORNER_RADIUS(32)});
        }
    }
    CLAY(CLAY_ID("HealthBar1"),{.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_LEFT_TOP}, .offset = {660, 395}}}){
        CLAY_TEXT(CLAY_STRING("Gamer125"),CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(0.87f), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(0.75f), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_YELLOW, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
    }

    CLAY(CLAY_ID("HealthBar2"),{.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_LEFT_TOP}, .offset = {1160, 60}}}){
        CLAY_TEXT(CLAY_STRING("Wooven"),CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(0.67f), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(1), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_YELLOW, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
    }

    CLAY(CLAY_ID("HealthBar3"),{.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_FIT(0), CLAY_SIZING_FIT(0) }, .childGap = 4, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_TOP}},
    .floating = { .attachTo = CLAY_ATTACH_TO_PARENT, .attachPoints = { CLAY_ATTACH_POINT_CENTER_TOP, CLAY_ATTACH_POINT_LEFT_TOP}, .offset = {1360, 480}}}){
        CLAY_TEXT(CLAY_STRING("Fizzy"),CLAY_TEXT_CONFIG({ .fontId = FONT_ID_BOGLE, .fontSize = 24, .textColor = COLOR_LIGHT, .userData = FrameAllocateCustomData((CustomHTMLData) { .disablePointerEvents = true }) }));
        CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(1), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_RED, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
            CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_FIXED(200), CLAY_SIZING_FIXED(12) }, .padding = {0,0,2,2}, .childAlignment = {CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_BOTTOM}},
             .border = {.color = COLOR_LIGHT, .width = {2,2,2,2}}, .cornerRadius = CLAY_CORNER_RADIUS(4)}){
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_PERCENT(0.12f), CLAY_SIZING_GROW(0) }}, .backgroundColor = COLOR_YELLOW, .cornerRadius = CLAY_CORNER_RADIUS(4)});
                CLAY_AUTO_ID({.layout = {.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}});
            }
    }
}


Clay_RenderCommandArray CreateLayout(bool mobileScreen, float lerpValue) {
    Clay_BeginLayout();
    switch (current_page)
    {
        case PAGE_MAIN:{
            CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .image = {&mainpage_wallpapers[mainpage_index]} }) {
                HeaderBar();
                CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
                    SideBar();
                    MainLayout();
                }
            }
        }break;
        case PAGE_CHARACTERS:{
            CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .image = {&background_image} }) {
                HeaderBar();
                CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
                    SideBar();
                    ChampionsLayout();
                }
            } 
        } break;
        case PAGE_CHARACTER_DRAFT:{
            CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .padding = CLAY_PADDING_ALL(DEFAULT_SPACING), .childGap = DEFAULT_SPACING }, .image = {&background_image} }) {
                HeaderBar();
                CLAY(CLAY_ID("PageContainer"), { .layout = { .layoutDirection = CLAY_LEFT_TO_RIGHT, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }, .childGap = DEFAULT_SPACING } }){
                    SideBar();
                    PlayMenu();
                }
            } 
        } break;
        case PAGE_IN_GAME:{
            CLAY(CLAY_ID("OuterContainer"), { .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, .sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) }}, .image = {&ingame_image} }) {
                InGameUI(); 
            }
        }break;
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
