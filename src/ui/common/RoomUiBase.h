#pragma once

#include "core/Scene.h"
#include "core/features/RoomFeature.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {

class RoomHost : public virtual MenuPageHost {
public:
    virtual void openRoomFood() = 0;
    virtual void openRoomShower() = 0;
    virtual bool changeHomeFurniture(uint16_t,bool) { return false; }
};

class FoodHost : public virtual MenuPageHost {
public:
    virtual bool selectPageFood(uint8_t index) = 0;
};

class RoomUiBase : public Scene {
public:
    explicit RoomUiBase(RoomHost& host) : host(host) {}
    bool showsNotices() const { return true; }
protected:
    RoomHost& host;
    void activateRoom(uint8_t index);
};

class FoodUiBase : public Scene {
public:
    explicit FoodUiBase(FoodHost& host) : host(host) {}
protected:
    FoodHost& host;
    void activateFood(uint8_t index);
};

}  // namespace Ui
