#pragma once

#include "core/Scene.h"
#include "core/features/ContactsFeature.h"
#include "ui/common/MenuPageHost.h"

namespace Ui {

class ContactsHost : public virtual MenuPageHost {
public:
    virtual bool contactVisiting(uint8_t slot) const = 0;
    virtual bool contactInviteLocked(uint8_t slot) const = 0;
    virtual void openContactStatus(uint8_t slot) = 0;
    virtual Features::Contacts::InviteResult invitePageContact(uint8_t slot) = 0;
    virtual void contactJoined() = 0;
    virtual bool deletePageContact(uint8_t slot) = 0;
};

class ContactsUiBase : public Scene {
public:
    explicit ContactsUiBase(ContactsHost& host) : host(host) {}
    uint8_t storageCursor = 0;
    uint8_t storageActionCursor = 0;
    bool storageActionOpen = false;
    bool storageInviteConfirmOpen = false;
    bool storageInviteConfirmYes = true;
    bool storageReleaseConfirmOpen = false;
    bool storageReleaseConfirmYes = false;

    void resetState();
    void enterState();
    void closePopups();
    uint8_t listItemCount() const;
    uint8_t actionCount() const;
    Features::Contacts::Action actionAt(uint8_t index) const;
    const char* actionLabel(uint8_t index) const;
    bool handleButton(const ButtonEvent& event);
    bool handleTap(int localTarget);
    const Game::GameState& state() const { return host.pageState(); }
    Features::Contacts::Capabilities contactCapabilities(uint8_t slot) const;
protected:
    ContactsHost& host;
    virtual bool backItemsVisible() const { return true; }
    virtual void resetListScroll() {}
};

}  // namespace Ui
