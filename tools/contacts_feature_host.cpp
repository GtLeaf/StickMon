#include <cassert>
#include <cstring>

#include "core/features/ContactsFeature.h"
#include "core/UiStrings.h"
#include "game/ContactRoster.h"
#include "ui/common/ComputerUiBase.h"
#include "ui/common/ContactsUiBase.h"

namespace {
void unchanged(const Game::GameState& state, const Game::GameState& before) {
    assert(std::memcmp(&state, &before, sizeof(state)) == 0);
}

Game::GameState makeState() {
    Game::GameState state;
    state.storageCount = 4;
    for (uint8_t slot = 0; slot < state.storageCount; ++slot) {
        auto& monster = state.storage[slot];
        monster.ivPacked = 101 + slot;
        monster.metAt = 200 + slot;
        monster.origin = Game::Origin::BEFRIENDED;
    }
    state.storage[1].origin = Game::Origin::HATCHED;
    state.teamCount = 1;
    state.team[0] = state.storage[0];
    state.team[0].speciesId = 2;
    state.team[0].hpCur = 17;
    return state;
}

class Host : public Ui::ContactsHost, public Ui::ComputerHost {
public:
    Game::GameState game = makeState();
    bool visiting[Game::STORAGE_CAP] = {};
    bool locked[Game::STORAGE_CAP] = {};
    int backs = 0, homes = 0, backTutorials = 0, navigationTutorials = 0;
    int socialOpens = 0, contactsOpens = 0, statusSlot = -1, inviteCalls = 0, deleteCalls = 0;
    int joinedNotifications = 0;
    const bool* actionOpen = nullptr;
    bool exploring = false, deletionAllowed = true;
    Features::Contacts::InviteResult result = Features::Contacts::InviteResult::REFUSED;
    const char* notice = nullptr;
    uint32_t duration = 0;
    const Game::GameState& pageState() const override { return game; }
    bool pageExploreContext() const override { return exploring; }
    void pageBack() override { ++backs; }
    void pageHome() override { ++homes; }
    void pageNotice(const char* value, uint32_t timeout) override { notice = value; duration = timeout; }
    void pageBackTutorial() override { ++backTutorials; }
    void pageNavigationTutorial() override { ++navigationTutorials; }
    void openComputerSocial() override { ++socialOpens; }
    void openComputerContacts() override { ++contactsOpens; }
    bool contactVisiting(uint8_t slot) const override { return slot < Game::STORAGE_CAP && visiting[slot]; }
    bool contactInviteLocked(uint8_t slot) const override { return slot < Game::STORAGE_CAP && locked[slot]; }
    void openContactStatus(uint8_t slot) override { statusSlot = slot; }
    Features::Contacts::InviteResult invitePageContact(uint8_t) override { ++inviteCalls; return result; }
    void contactJoined() override {
        assert(actionOpen && !*actionOpen);
        ++joinedNotifications;
    }
    bool deletePageContact(uint8_t slot) override {
        ++deleteCalls;
        return deletionAllowed && ContactRoster::deleteContact(game, slot, contactVisiting(slot));
    }
};

class Page : public Ui::ContactsUiBase {
public:
    using ContactsUiBase::ContactsUiBase;
    int scrollResets = 0;
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override {}
protected:
    void resetListScroll() override { ++scrollResets; }
};

class Computer : public Ui::ComputerUiBase {
public:
    using ComputerUiBase::ComputerUiBase;
    using ComputerUiBase::activateComputer;
    SceneUpdateResult update(uint32_t, float) override { return SceneUpdateResult::parked(); }
    void render() override {}
};
}

int main() {
    using namespace Features::Contacts;
    auto state = makeState();
    const auto before = state;
    const auto member = capabilities(state, 0, false, false);
    assert(member.valid && member.inTeam && !member.canDelete);
    assert(ContactRoster::liveMonsterForContact(state, 0) == &state.team[0]);
    assert(actionCount(member) == 2);
    assert(actionAt(member, 0) == Action::STATUS && actionAt(member, 1) == Action::BACK);
    const auto hatched = capabilities(state, 1, false, false);
    assert(hatched.valid && !hatched.inTeam && !hatched.canDelete);
    assert(actionCount(hatched) == 3 && actionAt(hatched, 1) == Action::INVITE);
    const auto external = capabilities(state, 2, false, false);
    assert(external.valid && external.canDelete && actionCount(external) == 4);
    assert(actionAt(external, 0) == Action::STATUS && actionAt(external, 1) == Action::INVITE);
    assert(actionAt(external, 2) == Action::DELETE && actionAt(external, 3) == Action::BACK);
    assert(actionAt(external, 255) == Action::BACK);
    const auto visiting = capabilities(state, 2, true, false);
    assert(visiting.visiting && !visiting.canDelete && actionCount(visiting) == 3);
    assert(capabilities(state, 2, false, true).canDelete);
    for (int index = state.storageCount; index <= 255; ++index) {
        const auto invalid = capabilities(state, static_cast<uint8_t>(index), true, true);
        assert(!invalid.valid && !invalid.inTeam && !invalid.visiting && !invalid.locked && !invalid.canDelete);
        assert(actionCount(invalid) == 0 && actionAt(invalid, 0) == Action::BACK);
        assert(inviteGate(state, static_cast<uint8_t>(index), true, true) == InviteGate::INVALID);
    }
    assert(inviteGate(state, 2, false, false) == InviteGate::READY);
    assert(inviteGate(state, 0, false, false) == InviteGate::IN_TEAM);
    assert(inviteGate(state, 2, true, true) == InviteGate::VISITING);
    assert(inviteGate(state, 2, false, true) == InviteGate::LOCKED);
    unchanged(state, before);
    state.teamCount = Game::TEAM_CAP;
    assert(inviteGate(state, 2, true, true) == InviteGate::TEAM_FULL);
    state.storageCount = 255;
    assert(!capabilities(state, Game::STORAGE_CAP, false, false).valid);

    Host host;
    Computer computer(host);
    computer.activateComputer(0); computer.activateComputer(1); computer.activateComputer(2);
    assert(host.socialOpens == 1 && host.contactsOpens == 1);
    Page page(host);
    host.actionOpen = &page.storageActionOpen;
    auto press = [&](uint8_t button, BtnAction action = BtnAction::PRESSED) {
        assert(page.handleButton({button, action}));
    };
    page.resetState();
    for (int i = 0; i <= host.game.storageCount; ++i) press(1);
    assert(page.storageCursor == 0 && page.scrollResets == 1);
    page.storageCursor = 2;
    press(0);
    assert(page.storageActionOpen && page.actionCount() == 4);
    assert(std::strcmp(page.actionLabel(2), Ui::Storage::ACTION_DELETE) == 0);
    press(1); // Invite is the second action.
    const auto beforeActions = host.game;
    host.game.teamCount = Game::TEAM_CAP; host.visiting[2] = host.locked[2] = true;
    press(0);
    assert(!page.storageInviteConfirmOpen && host.notice == Ui::Storage::TEAM_FULL_TOAST && host.duration == 1100);
    host.game.teamCount = 1;
    press(0);
    assert(host.notice == Ui::Storage::VISITING && !page.storageInviteConfirmOpen);
    host.visiting[2] = false;
    press(0);
    assert(host.notice == Ui::Storage::INVITE_LOCKED && !page.storageInviteConfirmOpen);
    host.locked[2] = false;
    press(0);
    assert(page.storageInviteConfirmOpen && page.storageInviteConfirmYes);
    press(1); press(0); // No never invokes the mutation callback.
    assert(!page.storageInviteConfirmOpen && page.storageInviteConfirmYes && host.inviteCalls == 0);
    unchanged(host.game, beforeActions);

    for (const auto result : {InviteResult::REFUSED, InviteResult::LOCKED, InviteResult::ALREADY_IN_TEAM,
            InviteResult::TEAM_FULL, InviteResult::INVALID, InviteResult::JOINED}) {
        page.storageActionOpen = page.storageInviteConfirmOpen = page.storageInviteConfirmYes = true;
        host.result = result;
        press(0);
        const bool closesAction = result == InviteResult::REFUSED || result == InviteResult::ALREADY_IN_TEAM || result == InviteResult::JOINED;
        assert(page.storageActionOpen == !closesAction);
        assert(page.storageInviteConfirmOpen == (result == InviteResult::JOINED));
    }
    assert(host.inviteCalls == 6);
    assert(host.joinedNotifications == 1);
    unchanged(host.game, beforeActions);
    page.resetState();
    page.storageCursor = 2; press(0); press(0);
    assert(host.statusSlot == 2 && !page.storageActionOpen);

    page.storageCursor = 3; press(0);
    page.storageActionCursor = 2; press(0);
    assert(page.storageReleaseConfirmOpen && !page.storageReleaseConfirmYes);
    press(0);
    assert(!page.storageReleaseConfirmOpen && page.storageActionOpen && host.deleteCalls == 0);
    press(0); press(1); press(0);
    assert(!page.storageActionOpen && !page.storageReleaseConfirmOpen);
    assert(host.deleteCalls == 1 && host.game.storageCount == 3 && page.storageCursor == 2);
    assert(ContactRoster::sameMonster(host.game.storage[0], host.game.team[0]));
    assert(host.game.storage[1].origin == Game::Origin::HATCHED);

    page.storageActionOpen = page.storageInviteConfirmOpen = true;
    press(1, BtnAction::LONG_PRESS);
    assert(!page.storageInviteConfirmOpen && page.storageActionOpen);
    page.storageReleaseConfirmOpen = true;
    press(1, BtnAction::LONG_PRESS);
    assert(!page.storageReleaseConfirmOpen && page.storageActionOpen);
    press(1, BtnAction::LONG_PRESS);
    assert(!page.storageActionOpen);
    press(1, BtnAction::LONG_PRESS);
    assert(host.backs == 1 && host.backTutorials == 4);
    press(0, BtnAction::LONG_PRESS);
    assert(host.homes == 1);
    host.exploring = true;
    press(1, BtnAction::LONG_PRESS);
    assert(host.backs == 2 && host.backTutorials == 4);
    page.closePopups();
    assert(!page.handleTap(-1) && !page.handleTap(200));
    assert(page.handleTap(0) && page.storageActionOpen);
    assert(page.handleTap(128) && host.statusSlot == 0);
    assert(host.navigationTutorials == 2);
}
