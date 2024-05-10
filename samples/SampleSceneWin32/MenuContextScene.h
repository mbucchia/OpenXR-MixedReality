#pragma once

#include <XrSceneLib/Scene.h>

struct MenuContext {
    std::string MenuText = "";
    const std::function<void()> Callback = nullptr;
};

class MenuContextScene : public engine::Scene {
public:
    MenuContextScene(engine::Context& context, std::string menuText, std::function<void()> callback = nullptr)
        : Scene(context)
        , MenuContext({std::move(menuText), callback ? callback : [this] { OnMenuClicked(); }}) {
    }

    const MenuContext* GetMenuContext() const {
        return &MenuContext;
    }

protected:
    MenuContext MenuContext;

    virtual void OnMenuClicked() {
        ToggleActive();
    }
};
