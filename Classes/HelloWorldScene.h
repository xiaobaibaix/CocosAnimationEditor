#ifndef __HELLOWORLD_SCENE_H__
#define __HELLOWORLD_SCENE_H__

#include "cocos2d.h"

class HelloWorld : public cocos2d::Scene
{
public:
    static cocos2d::Scene* createScene();

    virtual bool init();
    virtual void onEnter();
    virtual void onExit();

    void menuCloseCallback(cocos2d::Ref* pSender);

    CREATE_FUNC(HelloWorld);

private:
    void demoUGF();
    void demoImGui();
    void demoBehaviac();
};

#endif // __HELLOWORLD_SCENE_H__
