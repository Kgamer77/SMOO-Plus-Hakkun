#include "sead/heap/seadHeap.h"
#include "sead/prim/seadSafeString.h"

#include "al/Library/Base/StringUtil.h"
#include "al/Library/Camera/CameraUtil.h"
#include "al/Library/Controller/InputFunction.h"
#include "al/Library/LiveActor/ActorFlagFunction.h"
#include "al/Library/LiveActor/ActorMovementFunction.h"
#include "al/Library/LiveActor/ActorPoseUtil.h"
#include "al/Library/Message/MessageHolder.h"
#include "al/Library/Nerve/NerveUtil.h"
#include "al/Library/Scene/SceneObjUtil.h"

#include "game/Item/CoinCollectHolder.h"
#include "game/Item/ShineInfo.h"
#include "game/Player/HackCap.h"
#include "game/Player/PlayerActorHakoniwa.h"
#include "game/Player/PlayerAnimator.h"
#include "game/Player/PlayerFunction.h"
#include "game/Player/PlayerHackKeeper.h"
#include "game/Scene/StageScene.h"
#include "game/System/GameDataFile.h"
#include "game/System/GameDataFunction.h"
#include "game/System/GameDataHolderAccessor.h"
#include "game/System/GameDataHolderWriter.h"
#include "game/System/GameProgressData.h"
#include "game/System/WorldList.h"
#include "game/Util/ActorDimensionKeeper.h"
#include "game/Util/ObjUtil.h"
#include "game/Util/PlayerUtil.h"
#include "game/Util/StageLayoutFunction.h"

#include "basis/seadNew.h"
#include "helpers.hpp"
#include "imgui.h"
#include "logger.hpp"
#include "puppets/PuppetInfo.h"
#include "rs/util.hpp"
#include "Scene/StageSceneStateModConfig.hpp"
#include "server/archipelago/ArchipelagoConfigMenu.hpp"
#include "server/archipelago/ArchipelagoHelpers.hpp"
#include "server/archipelago/ArchipelagoMode.hpp"
#include "server/Client.hpp"
#include "server/DeltaTime.hpp"
#include "server/gamemode/GameModeBase.hpp"
#include "server/gamemode/GameModeFactory.hpp"
#include "server/gamemode/GameModeManager.hpp"
#include "System/PlayerHitPointData.h"

void ArchipelagoMode::isSubArea(GameDataHolderAccessor accessor, bool* isInSubArea, sead::FixedSafeString<32> stageId) {
    *isInSubArea = !GameDataFunction::isMainStage(accessor);

    // Handle Sub Area -> Sub Area connections
    if (*isInSubArea) {
        const char* subAreaExclusions[] = {"SnowWorldTown", "Underground000", "Special2WorldLavaStage", "Revenge"};
        for (int i = 0; i < sizeof(subAreaExclusions) / sizeof(subAreaExclusions[0]); i++) {
            if (isPartOf(GameDataFunction::getCurrentStageName(accessor), subAreaExclusions[i])) {
                // Stage Ids that lead into the sub area from an overworld
                // might need to add darker side transitions and dark side transitions
                // possibly re add Under01
                const char* excludedStageIds[] = {"SnowUG",       "CP_Entrance", "MoonGoal",         "MofumofuA", "BossMagmaA",
                                                  "BossKnuckleA", "BossForestA", "GiantWanderBossA", "BossRaidA"};
                *isInSubArea = false;
                for (int j = 0; j < sizeof(excludedStageIds) / sizeof(excludedStageIds[0]); j++) {
                    *isInSubArea = isPartOf(stageId.cstr(), excludedStageIds[j]);
                    if (*isInSubArea) {
                        break;
                    }
                }
                break;
            }
        }
    }
}

void ArchipelagoMode::getCustomStageId(GameDataHolderAccessor accessor, const ChangeStageInfo* info, sead::FixedSafeString<64>* stageId) {
    if (isPartOf(info->getStageName(), "LavaBonus") || isPartOf(GameDataFunction::getCurrentStageName(accessor), "LavaBonus")) {
        *stageId = "town_lava";
    }

    if (isPartOf(info->getStageName(), "LavaWorldShopStage") || isPartOf(GameDataFunction::getCurrentStageName(accessor), "LavaWorldShopStage")) {
        *stageId = "shop_lava";
    }

    if (isPartOf(stageId->cstr(), "aaa") && isPartOf(GameDataFunction::getCurrentStageName(accessor), "SandWorldHome")) {
        *stageId = "aaaSand";
    }

    if ((!isPartOf(stageId->cstr(), "A") && !isPartOf(stageId->cstr(), "B")) &&
        (isPartOf(info->getStageName(), "Revenge") || isPartOf(GameDataFunction::getCurrentStageName(accessor), "Picture"))) {
        *stageId = "PictureBoss";
        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Knuckle"))
            stageId->append("Knuckle");

        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Mofumofu"))
            *stageId = "PictureMofumofu";

        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Forest"))
            stageId->append("Forest");

        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Wander"))
            *stageId = "PictureGiantWanderBoss";

        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Raid"))
            stageId->append("Raid");

        if (isPartOf(GameDataFunction::getCurrentStageName(accessor), "Magma"))
            stageId->append("Magma");
    }
}

void ArchipelagoMode::correctCustomStageId(sead::FixedSafeString<64>* toStageId) {
    if (al::isEqualString(toStageId->cstr(), "town_lava")) {
        *toStageId = "town";
    }

    if (al::isEqualString(toStageId->cstr(), "shop_lava")) {
        *toStageId = "shop";
    }

    if (al::isEqualString(toStageId->cstr(), "aaaSand")) {
        *toStageId = "aaa";
    }
}

// Doesn't handle achieve
int ArchipelagoMode::getNumGotShines() {
    GameDataHolderAccessor accessor(mCurScene);
    int totalShines = GameDataFunction::getWorldTotalShineNum(accessor, mCurWorldShineList);
    int curShines = 0;
    for (int k = 0; k < 0x400; k++) {
        GameDataFile::HintInfo curHintInfo = accessor.mData->getGameDataFile()->getHintList()[k];
        if (curHintInfo.worldId == mCurWorldShineList && curHintInfo.isGet) {
            curShines += 1;
        }
    }

    return curShines;
}

int ArchipelagoMode::getNumCoinCollect() {
    GameDataHolderAccessor accessor(mCurScene);
    GameDataFile* gameDataFile = accessor.mData->getGameDataFile();
    return gameDataFile->getCoinCollectGotNum(mRelativeWorldCoinCollect) - gameDataFile->getUseCoinCollectNum(mRelativeWorldCoinCollect);
}

void ArchipelagoMode::setRelativeWorldCoinCollect(const char* stageName) {
    GameDataHolderAccessor accessor(mCurScene);
    sead::FixedSafeString<64> toStageCoinCollect = sead::FixedSafeString<64>();
    if (isPartOf(stageName, "WorldShop")) {
        toStageCoinCollect = stageName;
        toStageCoinCollect.replaceString("WorldShopStage", "");
        toStageCoinCollect.replaceString("WorldShop01Stage", "");
    } else {
        toStageCoinCollect = getWorldStageNameByRegionalCoinStageList(stageName);
        toStageCoinCollect.replaceString("WorldHomeStage", "");
    }
    mRelativeWorldCoinCollect = GameDataFunction::findWorldIdByDevelopName(accessor, toStageCoinCollect.cstr());
}

// Death Link handling
void ArchipelagoMode::handleDeathLink(PlayerActorBase* playerBase, PlayerActorHakoniwa* playerHakoniwa, GameDataHolderWriter writer) {
    if (!PlayerFunction::isPlayerDeadStatus(playerBase) && mApDeath) {
        GameDataFunction::killPlayer(writer);
        playerBase->startDemoPuppetable();
        al::setVelocityZero(playerBase);
        rs::faceToCamera(playerBase);
        playerHakoniwa->mAnimator->endSubAnim();
        playerHakoniwa->mAnimator->startAnimDead();
        mApDeath = false;
    }

    if (PlayerFunction::isPlayerDeadStatus(playerBase) && !mDying) {
        if (mDeathLinkEnabled)
            Client::sendDeathlinkPacket();
        mDying = true;
    }

    if (!PlayerFunction::isPlayerDeadStatus(playerBase) && mDying) {
        mDying = false;
    }
}

// Capture Sanity Enforcement
void ArchipelagoMode::handleCaptureSanity(PlayerActorBase* playerBase, GameDataHolderAccessor accessor) {
    if (mCapturesEnabled) {
        al::LiveActor* curHack = playerBase->getPlayerHackKeeper()->mHackModel;
        const char* hackName = playerBase->getPlayerHackKeeper()->getCurrentHackName();
        if (hackName != nullptr && !hasCapture(hackName) && mIsRecordCapture) {
            if (!(al::isEqualString(hackName, "ElectricWire") && getScenario(0) < 2 && GameDataFunction::getCurrentWorldId(accessor) == 0)) {
                // Client::Client::addMessage(hackNamehackName);
                if (!playerBase->getPlayerHackKeeper()->isActiveHackStartDemo()) {
                    bool tryEscape = false;
                    // 29 (pole) removed
                    int nonKillCaptures[7] = {10, 13, 24, 25, 28, 37};
                    for (int i = 0; i < 7; i++) {
                        tryEscape = al::isEqualString(captureListNames[nonKillCaptures[i]], hackName);
                        if (tryEscape) {
                            break;
                        }
                    }
                    if (tryEscape) {
                        playerBase->getPlayerHackKeeper()->tryEscapeHack();
                    } else {
                        playerBase->getPlayerHackKeeper()->forceKillHack();
                    }
                    mIsRecordCapture = false;
                }
            }
        }
    }
}

void ArchipelagoMode::getNearestRegional(StageScene* stageScene, PlayerActorBase* playerBase) {
    if (al::isExistSceneObj(stageScene, 7)) {
        CoinCollectHolder* coinCollectHolder = (CoinCollectHolder*)al::getSceneObj(stageScene, 7);
        if (coinCollectHolder) {
            CoinCollect* coinCollect = coinCollectHolder->tryFindAliveCoinCollect(al::getTrans(playerBase), true);
            if (coinCollect) {
                mHintArrow->setTarget(al::getTransPtr(coinCollect));
                mCoinCollectHintTarget = coinCollect;
                coinCollect->appearHelpAmiiboEffect();
            } else {
                CoinCollect2D* coinCollect2D = coinCollectHolder->tryFindAliveCoinCollect2D(al::getTrans(playerBase), true);

                if (coinCollect2D) {
                    mHintArrow->setTarget(al::getTransPtr(coinCollect2D));
                    mCoinCollectHintTarget = coinCollect2D;
                    coinCollect2D->appearHintEffect();
                }
            }
        }
    }
}

void ArchipelagoMode::handleSoftLocks(GameDataHolderAccessor accessor, GameDataHolderWriter writer) {
    // softlock prevention
    // Doesn't fix soft lock...
    // if (gameProgressData->mWaterfallWorldProgress != GameProgressData::WaterfallWorldProgress::TalkedCapNearHome) {
    //     gameProgressData->mWaterfallWorldProgress = GameProgressData::WaterfallWorldProgress::TalkedCapNearHome;
    // }

    if (mSoftlockTimer >= 60) {
        GameProgressData* gameProgressData = accessor.mData->getGameDataFile()->getGameProgressData();

        for (int i = 0; i < mStoryShineArray.size(); i++) {
            GameDataFile::HintInfo curHintInfo = accessor.mData->getGameDataFile()->getHintList()[mStoryShineArray[i]->mShineIdx];
            if (mStoryShineArray[i] && hasShine(curHintInfo.uniqueId) || curHintInfo.uniqueId == 205 && hasShine(shineScenarios[0].shineUid)) {
                mStoryShineArray[i]->onSwitchGet();
            }
        }

        if (accessor.mData->getGameDataFile()->isUseMissRestartInfo()) {
            accessor.mData->getGameDataFile()->setIsUseMissRestartInfo(false);
        }

        // Lost and Ruined Odyssey softlock handling
        if (gameProgressData->mHomeStatus == GameProgressData::HomeStatus::CrashedHome ||
            gameProgressData->mHomeStatus == GameProgressData::HomeStatus::BossAttackedHome) {
            gameProgressData->mHomeStatus = GameProgressData::HomeStatus::LaunchedHome;
        }
        mSoftlockTimer = 0;
    }
}

void ArchipelagoMode::updateCounter(PlayerActorBase* playerBase, GameDataHolderAccessor accessor) {
    // Moon Shard Updater
    // Prevents softlock when moon is received mid shard moon
    if (!(al::isEqualString(GameDataFunction::tryGetCurrentMainStageName(accessor), "CapWorldHomeStage") && getScenario(0) < 2) &&
        rs::isExistShineChipWatcher(playerBase) && rs::getShineChipCount(playerBase) > 0) {
        Client::startShineChipCount();
    }

    if (mUpdateCounterTimer >= 1800) {
        if (mIsNeedUpdateCounter) {
            Client::startShineCount();
            mIsNeedUpdateCounter = false;
        }
        mUpdateCounterTimer = 0;
    }
}

int ArchipelagoMode::getRelativeWorldCoinCollectCheckGotNum(GameDataHolderAccessor accessor) {
    sead::FixedSafeString<64> currentWorldStageName = sead::FixedSafeString<64>();
    currentWorldStageName = GameDataFunction::getWorldDevelopName(accessor, mRelativeWorldCoinCollect);
    currentWorldStageName.append("WorldHomeStage");

    int indexCurrentHomeStage = getIndexRegionalCoinStageList(currentWorldStageName.cstr());

    int regionalCoinCheckGotNum = 0;

    int index = 0;
    for (int k = 0; k < indexCurrentHomeStage; k++) {
        index += regionalCoinListLengths[k];
    }

    for (size_t i = indexCurrentHomeStage; i < sizeof(regionalCoinStages) / sizeof(regionalCoinStages[0]); i++) {
        if (i != indexCurrentHomeStage && isPartOf(regionalCoinStages[i], "WorldHomeStage"))
            break;

        for (size_t j = 0; j < regionalCoinListLengths[i]; j++) {
            if (hasRegionalCoin(index + j)) {
                regionalCoinCheckGotNum += 1;
            }
        }
        index += regionalCoinListLengths[i];
    }

    return regionalCoinCheckGotNum;
}

void ArchipelagoMode::calculateShineScenarios() {
    for (int i = 0; i < 14; i++) {
        if (hasShine(shineScenarios[i].shineUid)) {
            setScenario(shineScenarios[i].worldId, shineScenarios[i].scenario);
        }
    }
}

int ArchipelagoMode::isMoonRockScenario(int worldId) {
    GameDataHolderAccessor accessor(mCurScene);
    int curScenario = getScenario(worldId);
    int preMoonRockScenario = accessor.mData->mWorldList->getAfterEndingScenarioNo(worldId);
    if (curScenario >= preMoonRockScenario)
        return curScenario;
    bool isMoonRock = true;
    if (worldId < GameDataFunction::getWorldIndexPeach()) {
        for (int i = 0; i < storyLengths[worldId]; i++) {
            short shineId = storyShines[worldId][i];
            if (shineId > 0 && !hasShine(shineId)) {
                isMoonRock = false;
                break;
            }
            if (shineId == -1) {
                if (curScenario < 2 && !GameDataFunction::isUnlockedWorld(accessor, worldId)) {
                    curScenario = 2;
                    setScenario(worldId, curScenario);
                } else {
                    curScenario = 1;
                    setScenario(worldId, curScenario);
                }
                isMoonRock = mDefeatedBowserInCloud;
            }
            if (shineId == -2) {
            }
        }
    }
    if (isMoonRock) {
        setScenario(worldId, preMoonRockScenario);
        return preMoonRockScenario;
    }
    return curScenario;
}

int ArchipelagoMode::getSubAreaScenario(const char* toStageName) {
    int toScenario = -1;
    if (al::isEqualString(toStageName, "ForestWorldBossStage"))
        toScenario = getScenario(GameDataFunction::getWorldIndexForest()) > 2 ? 2 : 1;

    if (al::isEqualString(toStageName, "ForestWorldWoodsStage"))
        toScenario = getScenario(GameDataFunction::getWorldIndexForest()) > 1 ? 2 : 1;

    if (al::isEqualString(toStageName, "CapWorldTowerStage"))
        toScenario = 2;

    return toScenario;
}

bool ArchipelagoMode::tryShowCappyMessage(StageScene* curScene) {
    al::IUseSceneObjHolder* curObjHolder = (al::IUseSceneObjHolder*)curScene;

    // Stop MessageBuffer from being edited or read before ready
    // Start read prevention in buildCappyMessage
    // End read prevention at tryShow
    bool isActive = rs::isActiveCapMessage(curObjHolder);
    if (isActive) {
        mCappyMessageFrameTimer = 100;
    }

    if (mCappyMessageFrameTimer > 0 || mCappyBufferInUse || mCappyMessages[mCurrentCappyMessage].itemType == 255) {
        if (!isActive) {
            mCappyMessageFrameTimer -= 1;
            if (mCappyMessageFrameTimer <= 0)
                mCappyBufferInUse = false;
        }
        return false;
    }

    buildCappyMessage();

    if (rs::tryShowCapMessagePriorityLow(curObjHolder, kArchipelagoCappyLabel, 180, 0)) {
        // Reset the read message
        sead::FixedSafeString<64> debugStr = sead::FixedSafeString<64>();
        debugStr = "Current Cappy Message: Slot: ";
        debugStr.append(intToCstr(mCappyMessages[mCurrentCappyMessage].slotNameIndex));
        debugStr.append(" , Type: ");
        debugStr.append(intToCstr(mCappyMessages[mCurrentCappyMessage].itemType));
        debugStr.append(" , Item: ");
        debugStr.append(intToCstr(mCappyMessages[mCurrentCappyMessage].itemIndex));
        Client::addMessage(debugStr.cstr());
        mCappyMessages[mCurrentCappyMessage] = {255, 255, 255, false};
        mCurrentCappyMessage = (mCurrentCappyMessage + 1) % kCappyMessageQueueSize;
        // mNextCappyMessage = (mNextCappyMessage + 1) % kCappyMessageQueueSize;
        return true;
    }

    return false;
}

void ArchipelagoMode::buildCappyMessage() {
    if (mCappyBufferInUse)
        return;
    mCappyBufferInUse = true;
    cappyMessage curMessage = mCappyMessages[mCurrentCappyMessage];
    // const char* tags[] = {"<name>", "<item>", "<type>", ""};
    GameDataHolderAccessor accessor(mCurScene);
    short tagIndex = 0;

    bool hasName = !mSlotNames[curMessage.slotNameIndex].isEmpty();
    if (curMessage.itemType == 0) {
        mSafeCappyBuffer = u"Connected to Archipelago as ";
    } else {
        if (curMessage.isOutgoing) {
            mSafeCappyBuffer = u"Sent ";
        } else {
            if (hasName)
                mSafeCappyBuffer = u"Got ";
            else
                mSafeCappyBuffer = u"Found ";
        }

        if (curMessage.itemType < CappyMessageTypes::CappyRegionalCoin) {
            getColor(ProjectTextColors::Salmon, &mSafeCappyBuffer, tagIndex);
            tagIndex++;
            appendUtf8ToUtf16(kingdoms[curMessage.itemIndex], &mSafeCappyBuffer);
        }

        else if (curMessage.itemType == CappyMessageTypes::CappyRegionalCoin) {
            // appendUtf8ToUtf16(kingdoms[curMessage.itemIndex], &mSafeCappyBuffer);
            mSafeCappyBuffer.append(getRegionalCoinIcon(curMessage.itemIndex));
            tagIndex++;
        }

        else if (curMessage.itemType <= CappyMessageTypes::CappyCapture) {
            appendUtf8ToUtf16(mItemNames[curMessage.itemIndex].cstr(), &mSafeCappyBuffer);
        }

        else if (curMessage.itemType == CappyMessageTypes::CappyCoins) {
            appendUtf8ToUtf16(intToCstr(curMessage.itemIndex), &mSafeCappyBuffer);
            mSafeCappyBuffer.append(u" ");
            mSafeCappyBuffer.append(getMiscIcon(1));
        }

        mSafeCappyBuffer.append(u" ");
        getColor(ProjectTextColors::Plum, &mSafeCappyBuffer, tagIndex);
        tagIndex++;
        appendUtf8ToUtf16(SMOItems[curMessage.itemType], &mSafeCappyBuffer);
        getColor(ProjectTextColors::ResetColor, &mSafeCappyBuffer, tagIndex);
        tagIndex++;
        if (hasName) {
            if (curMessage.isOutgoing)
                mSafeCappyBuffer.append(u" to ");
            else
                mSafeCappyBuffer.append(u" from ");
        }
    }

    if (hasName) {
        getColor(ProjectTextColors::Magenta, &mSafeCappyBuffer, tagIndex);
        tagIndex++;
        appendUtf8ToUtf16(mSlotNames[curMessage.slotNameIndex].cstr(), &mSafeCappyBuffer);
        getColor(ProjectTextColors::ResetColor, &mSafeCappyBuffer, tagIndex);
        tagIndex++;
    }

    int replacedSpaceIndex = 0;
    for (int i = 0; i < mSafeCappyBuffer.calcLength(); i++) {
        if (mSafeCappyBuffer[i] == u' ') {
            replacedSpaceIndex = i;
        }
        if (i > 32) {
            mSafeCappyBuffer.getBuffer()[replacedSpaceIndex] = u'\n';
            break;
        }
    }
    getColor(ProjectTextColors::ResetColor, &mSafeCappyBuffer, tagIndex);
    mSafeCappyBuffer.append(u".");
    mSafeCappyBuffer.replaceChar(u'\xf777', u'\x0000');

    return;
}
