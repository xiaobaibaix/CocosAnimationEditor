#include "HelloWorldScene.h"
#include "SimpleAudioEngine.h"

// 第三方库头文件
#include "UGF.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "behaviac/behaviac.h"

USING_NS_CC;

Scene* HelloWorld::createScene()
{
    return HelloWorld::create();
}

static void problemLoading(const char* filename)
{
    CCLOG("Error while loading: %s", filename);
}

bool HelloWorld::init()
{
    if (!Scene::init()) { return false; }

    auto visibleSize = Director::getInstance()->getVisibleSize();
    Vec2 origin = Director::getInstance()->getVisibleOrigin();

    auto closeItem = MenuItemImage::create(
        "CloseNormal.png", "CloseSelected.png",
        CC_CALLBACK_1(HelloWorld::menuCloseCallback, this));

    if (closeItem && closeItem->getContentSize().width > 0)
    {
        closeItem->setPosition(Vec2(
            origin.x + visibleSize.width - closeItem->getContentSize().width/2,
            origin.y + closeItem->getContentSize().height/2));
    }

    auto menu = Menu::create(closeItem, NULL);
    menu->setPosition(Vec2::ZERO);
    this->addChild(menu, 1);

    auto label = Label::createWithTTF(
        "Hello + UGF + ImGui + Behaviac", "fonts/Marker Felt.ttf", 24);
    if (label)
    {
        label->setPosition(Vec2(origin.x + visibleSize.width/2,
                                origin.y + visibleSize.height - label->getContentSize().height));
        this->addChild(label, 1);
    }

    auto sprite = Sprite::create("HelloWorld.png");
    if (sprite)
    {
        sprite->setPosition(Vec2(visibleSize.width/2 + origin.x,
                                 visibleSize.height/2 + origin.y));
        this->addChild(sprite, 0);
    }
    return true;
}

void HelloWorld::onEnter()
{
    Scene::onEnter();
    CCLOG("=== 第三方库演示开始 ===");
    demoUGF();
    demoImGui();
    demoBehaviac();
    CCLOG("=== 第三方库演示完成 ===");
}

void HelloWorld::onExit()
{
    Scene::onExit();
}

// ============================================================
//  UGF 框架演示
// ============================================================
void HelloWorld::demoUGF()
{
    CCLOG("--- [UGF] Universal Game Framework 演示 ---");

    // 1. 初始化 UGF
    auto& app = ugf::UGF::getInstance();
    if (!app.initialize())
    {
        CCLOG("[UGF] init failed!");
        return;
    }
    CCLOG("[UGF] init OK, version=%s", UGF_VERSION_STRING);

    // 2. ConfigSystem - 加载 JSON 配置
    auto& config = app.getConfig();
    config.loadString("{\"game\":{\"title\":\"CocosAnimationEditor\",\"version\":\"1.0\"}}");
    const auto* sec = config.getSection("game");
    if (sec)
    {
        CCLOG("[UGF] Config: title=%s",
              sec->get("title", std::string("?")).c_str());
    }

    // 3. EventBus - 事件发布/订阅
    struct TestEvent { int value; };
    auto conn = ugf::EventBus::getInstance().subscribe<TestEvent>(
        [](const TestEvent& e) {
            CCLOG("[UGF] received TestEvent value=%d", e.value);
        });
    ugf::EventBus::getInstance().publish(TestEvent{42});
    CCLOG("[UGF] EventBus OK");

    CCLOG("[UGF] 演示完成");
}

// ============================================================
//  ImGui 库演示
// ============================================================
void HelloWorld::demoImGui()
{
    CCLOG("--- [ImGui] Dear ImGui 演示 ---");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    CCLOG("[ImGui] context created, version=%s", IMGUI_VERSION);

    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    CCLOG("[ImGui] DisplaySize=%.0fx%.0f", io.DisplaySize.x, io.DisplaySize.y);

    // ImVector
    ImVector<int> vec;
    vec.push_back(10); vec.push_back(20); vec.push_back(30);
    CCLOG("[ImGui] ImVector size=%d [1]=%d", vec.Size, vec[1]);

    // ImDrawList (需要 imgui_internal.h)
    ImDrawListSharedData sharedData;
    ImDrawList drawList(&sharedData);
    drawList.AddRectFilled({0, 0}, {100, 100}, IM_COL32(255, 0, 0, 255));
    CCLOG("[ImGui] DrawList Vtx=%d Idx=%d",
          drawList.VtxBuffer.Size, drawList.IdxBuffer.Size);

    CCLOG("[ImGui] 演示完成");
}

// ============================================================
//  Behaviac 行为树库演示
// ============================================================
void HelloWorld::demoBehaviac()
{
    CCLOG("--- [Behaviac] Behaviac 演示 ---");

    // Workspace 单例
    auto* ws = behaviac::Workspace::GetInstance();
    CCLOG("[Behaviac] Workspace OK (%p)", static_cast<void*>(ws));

    // 字符串类型
    behaviac::string btStr = "BehaviorTreeExample";
    CCLOG("[Behaviac] string: %s", btStr.c_str());

    // CRC 工具
    behaviac::CStringCRC crc("TestNode");
    CCLOG("[Behaviac] CStringCRC id=%u", crc.GetUniqueID());

    CCLOG("[Behaviac] 演示完成");
}

void HelloWorld::menuCloseCallback(Ref* pSender)
{
    Director::getInstance()->end();
}
