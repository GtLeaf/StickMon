#include "ui/common/ContactsUiBase.h"

#include "core/UiStrings.h"

namespace Ui {

void ContactsUiBase::resetState() {
    storageCursor = storageActionCursor = 0;
    storageInviteConfirmYes = true;
    storageReleaseConfirmYes = false;
    closePopups();
}

void ContactsUiBase::enterState() {
    storageCursor = storageActionCursor = 0;
    storageActionOpen = false;
    storageReleaseConfirmOpen = false;
}

void ContactsUiBase::closePopups() {
    storageActionOpen = storageInviteConfirmOpen = storageReleaseConfirmOpen = false;
}

Features::Contacts::Capabilities ContactsUiBase::contactCapabilities(uint8_t slot) const {
    return Features::Contacts::capabilities(state(), slot,
        host.contactVisiting(slot), host.contactInviteLocked(slot));
}

uint8_t ContactsUiBase::actionCount() const {
    const auto count = Features::Contacts::actionCount(contactCapabilities(storageCursor));
    return count && !backItemsVisible() ? count - 1 : count;
}

uint8_t ContactsUiBase::listItemCount() const {
    return state().storageCount + (backItemsVisible() ? 1 : 0);
}

Features::Contacts::Action ContactsUiBase::actionAt(uint8_t index) const {
    return Features::Contacts::actionAt(contactCapabilities(storageCursor), index);
}

const char* ContactsUiBase::actionLabel(uint8_t index) const {
    using Features::Contacts::Action;
    switch (actionAt(index)) {
    case Action::STATUS: return Ui::Storage::ACTION_STATUS;
    case Action::INVITE: return Ui::Storage::ACTION_INVITE;
    case Action::DELETE: return Ui::Storage::ACTION_DELETE;
    case Action::BACK: return Ui::Storage::ACTION_BACK;
    }
    return Ui::Storage::ACTION_BACK;
}

bool ContactsUiBase::handleButton(const ButtonEvent& event) {
    if (event.btn == 0 && event.action == BtnAction::LONG_PRESS) {
        host.pageHome();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::LONG_PRESS) {
        if (!host.pageExploreContext()) host.pageBackTutorial();
        if (storageInviteConfirmOpen) storageInviteConfirmOpen = false;
        else if (storageReleaseConfirmOpen) storageReleaseConfirmOpen = false;
        else if (storageActionOpen) {
            storageActionOpen = false;
            storageReleaseConfirmOpen = false;
        } else host.pageBack();
        return true;
    }
    if (event.btn == 1 && event.action == BtnAction::PRESSED) {
        if (storageInviteConfirmOpen) storageInviteConfirmYes = !storageInviteConfirmYes;
        else if (storageReleaseConfirmOpen) storageReleaseConfirmYes = !storageReleaseConfirmYes;
        else if (storageActionOpen) {
            const auto count = actionCount();
            if (!count) closePopups();
            else storageActionCursor = (storageActionCursor + 1) % count;
        } else {
            const uint8_t count = listItemCount();
            if (!count) return true;
            storageCursor = (storageCursor + 1) % count;
            if (storageCursor == 0) resetListScroll();
        }
        return true;
    }
    if (event.btn != 0 || event.action != BtnAction::PRESSED) return false;
    if (storageInviteConfirmOpen) {
        if (storageInviteConfirmYes) {
            using Features::Contacts::InviteResult;
            const auto result = host.invitePageContact(storageCursor);
            if (result == InviteResult::JOINED) {
                storageActionOpen = false;
                host.contactJoined();
                return true;
            }
            if (result == InviteResult::REFUSED || result == InviteResult::ALREADY_IN_TEAM)
                storageActionOpen = false;
        }
        storageInviteConfirmOpen = false;
        storageInviteConfirmYes = true;
        return true;
    }
    if (storageReleaseConfirmOpen) {
        if (storageReleaseConfirmYes) {
            if (host.deletePageContact(storageCursor)) {
                const auto count = state().storageCount;
                if (count == 0) storageCursor = 0;
                else if (storageCursor >= count) storageCursor = count - 1;
            }
            storageActionOpen = false;
        }
        storageReleaseConfirmOpen = false;
        return true;
    }
    if (storageActionOpen) {
        if (!contactCapabilities(storageCursor).valid) {
            storageActionOpen = false;
            return true;
        }
        using Features::Contacts::Action;
        const auto action = actionAt(storageActionCursor);
        if (action == Action::STATUS) {
            storageActionOpen = false;
            host.openContactStatus(storageCursor);
        } else if (action == Action::INVITE) {
            using Features::Contacts::InviteGate;
            const auto gate = Features::Contacts::inviteGate(state(), storageCursor,
                host.contactVisiting(storageCursor), host.contactInviteLocked(storageCursor));
            const char* notice = nullptr;
            if (gate == InviteGate::TEAM_FULL) notice = Ui::Storage::TEAM_FULL_TOAST;
            else if (gate == InviteGate::VISITING) notice = Ui::Storage::VISITING;
            else if (gate == InviteGate::LOCKED) notice = Ui::Storage::INVITE_LOCKED;
            else if (gate == InviteGate::IN_TEAM) notice = Ui::Storage::IN_TEAM;
            if (notice) host.pageNotice(notice, 1100);
            else if (gate == InviteGate::READY) {
                storageInviteConfirmOpen = true;
                storageInviteConfirmYes = true;
            }
        } else if (action == Action::DELETE) {
            storageReleaseConfirmOpen = true;
            storageReleaseConfirmYes = false;
        } else storageActionOpen = false;
        return true;
    }
    if (storageCursor >= state().storageCount) {
        if (!backItemsVisible()) return false;
        host.pageBack();
        storageActionOpen = false;
        storageReleaseConfirmOpen = false;
    } else {
        storageActionOpen = true;
        storageActionCursor = 0;
        storageInviteConfirmOpen = storageReleaseConfirmOpen = false;
    }
    return true;
}

bool ContactsUiBase::handleTap(int target) {
    if (target < 0) return false;
    if (target >= 200) {
        const bool yes = target == 200;
        if (storageReleaseConfirmOpen) storageReleaseConfirmYes = yes;
        else if (storageInviteConfirmOpen) storageInviteConfirmYes = yes;
        else return false;
    } else if (target >= 128) {
        if (!storageActionOpen) return false;
        storageActionCursor = target - 128;
    } else storageCursor = target;
    host.pageNavigationTutorial();
    return handleButton({0, BtnAction::PRESSED});
}

}  // namespace Ui
