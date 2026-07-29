#pragma once
#include "hk/hook/Replace.h"
#include "hk/hook/Trampoline.h"

#include "sead/prim/seadSafeString.h"

#include "al/Library/Message/MessageHolder.h"

#include "eui/MessageMgr.h"
#include "server/archipelago/ArchipelagoMode.hpp"
#include "server/gamemode/GameModeManager.hpp"

// ----- Cappy Messenger: text-system intercept -----
//
// Four trampolines on al's per-mstxt-file message accessors. When
// CapMessageLayout::exeDelay (called from rs::tryShowCapMessagePriorityLow
// downstream) asks for ArchipelagoMode::kArchipelagoCappyLabel and a Cappy
// buffer is currently live, return our UTF-16 buffer and synthesize the
// "label exists" probe. All four are hooked because exeDelay dispatches
// through either the System or Stage variant based on
// CapMessageShowInfo::isStageMessage; rs::tryShowCapMessagePriorityLow uses
// the System path but defensive hooking of both costs little and protects
// against future code that uses the Stage path.

static bool isExistCappyLabelInSystemMessageHook(const al::IUseMessageSystem* sys, const char* mstxt, const char* label) {
    if (GameModeManager::instance() && GameModeManager::instance()->isModeAndActive(GameMode::ARCHIPELAGO)) {
        if (GameModeManager::instance()->getMode<ArchipelagoMode>()->lookupCappyMessageSubstitution(label) != nullptr) {
            return true;
        }
    }
    return al::isExistLabelInSystemMessage(sys, mstxt, label);
}

static const char16_t* getSystemMessageCappyStringHook(const al::IUseMessageSystem* sys, const char* mstxt, const char* label) {
    if (GameModeManager::instance() && GameModeManager::instance()->isModeAndActive(GameMode::ARCHIPELAGO)) {
        const char16_t* sub = GameModeManager::instance()->getMode<ArchipelagoMode>()->lookupCappyMessageSubstitution(label);
        if (sub)
            return sub;
    }
    return al::getSystemMessageString(sys, mstxt, label);
}

static bool isExistCappyLabelInStageMessageHook(const al::IUseMessageSystem* sys, const char* mstxt, const char* label) {
    if (GameModeManager::instance() && GameModeManager::instance()->isModeAndActive(GameMode::ARCHIPELAGO)) {
        if (GameModeManager::instance()->getMode<ArchipelagoMode>()->lookupCappyMessageSubstitution(label) != nullptr) {
            return true;
        }
    }
    return al::isExistLabelInStageMessage(sys, mstxt, label);
}

static const char16_t* getStageMessageCappyStringHook(const al::IUseMessageSystem* sys, const char* mstxt, const char* label) {
    if (GameModeManager::instance() && GameModeManager::instance()->isModeAndActive(GameMode::ARCHIPELAGO)) {
        const char16_t* sub = GameModeManager::instance()->getMode<ArchipelagoMode>()->lookupCappyMessageSubstitution(label);
        if (sub)
            return sub;
    }
    return al::getStageMessageString(sys, mstxt, label);
}

static const sead::Color4u8* MAGENTA = new sead::Color4u8(0xEE, 0x00, 0xEE, 0xFF);
static const sead::Color4u8* CYAN = new sead::Color4u8(0x00, 0xEE, 0xEE, 0xFF);
static const sead::Color4u8* SLATE_BLUE = new sead::Color4u8(0x6D, 0x8B, 0xE8, 0xFF);
static const sead::Color4u8* PLUM = new sead::Color4u8(0xAF, 0x99, 0xEF, 0xFF);
static const sead::Color4u8* SALMON = new sead::Color4u8(0xAF, 0x80, 0x72, 0xFF);
static const sead::Color4u8* ORANGE = new sead::Color4u8(0xFF, 0x77, 0x00, 0xFF);

static void setCustomGradationColors(eui::MessageMgr* mgr, uint index, sead::Color4u8 topColor, sead::Color4u8 bottomColor) {
    // Rough Color: (7, #50A0D2FF, #50A0D2FF)
    mgr->setGradationColor(index, topColor, bottomColor);

    // Can't check if Archipelago is enabled, this function is only run once
    mgr->setGradationColor(8, *MAGENTA, *MAGENTA);
    mgr->setGradationColor(9, *CYAN, *CYAN);
    mgr->setGradationColor(10, *SLATE_BLUE, *SLATE_BLUE);
    mgr->setGradationColor(11, *PLUM, *PLUM);
    mgr->setGradationColor(12, *SALMON, *SALMON);
    mgr->setGradationColor(13, *ORANGE, *ORANGE);
}
