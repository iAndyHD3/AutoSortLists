#include <Geode/modify/LevelListLayer.hpp>
#include <span>
#include "Geode/Enums.hpp"
#include "Geode/binding/ButtonSprite.hpp"
#include "Geode/binding/CCMenuItemSpriteExtra.hpp"
#include "Geode/binding/CCMenuItemToggler.hpp"
#include "Geode/binding/GJDifficultySprite.hpp"
#include "Geode/binding/GJGameLevel.hpp"
#include "Geode/binding/GJLevelList.hpp"
#include "Geode/binding/LevelListLayer.hpp"
#include "Geode/cocos/cocoa/CCObject.h"
#include "Geode/cocos/label_nodes/CCLabelBMFont.h"
#include "Geode/ui/BasedButtonSprite.hpp"
#include "Geode/ui/Layout.hpp"
#include "Geode/utils/cocos.hpp"
#include "Geode/utils/string.hpp"


using namespace cocos2d;

using namespace geode::log;
using geode::cocos::CCArrayExt;

enum class Method { Stars, Id, LevelName, AuthorName, Rating, Last = Rating };

const char* getMethodBtnStr(Method e) {
    switch (e) {
        case Method::Stars: return "Stars";
        case Method::Id: return "ID";
        case Method::LevelName: return "Level Name";
        case Method::AuthorName: return "Author Name";
        case Method::Rating: return "Featured";
    }
}

enum class LevelRating { None, StarOnly, Featured, Epic, Legendary, Mythic };

LevelRating getRating(GJGameLevel* level) {
    switch (level->m_isEpic) {
        case 1: return LevelRating::Epic;
        case 2: return LevelRating::Legendary;
        case 3: return LevelRating::Mythic;
        default: break;
    }
    if (level->m_featured != 0)
        return LevelRating::Featured;
    if (level->m_stars.value() != 0)
        return LevelRating::StarOnly;
    return LevelRating::None;
}

int getDiffSpriteAverage(GJGameLevel* level) {
    if (level->m_demon.value() == 1) {
        switch (level->m_demonDifficulty) {
            case 3: return 7;
            case 4: return 8;
            case 5: return 9;
            case 6: return 10;
            default: return 6;
        }
    }
    if (level->m_autoLevel) {
        return -1;
    }
    return level->getAverageDifficulty();
}

struct Sorter {
    virtual bool canSplit() { return false; }
    virtual std::vector<std::span<int>> split(const gd::vector<int>&) { return {}; }
    // virtual void sort(gd::vector<int>&);

    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) = 0;
};


struct SortById : Sorter {
    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) {
        return left->m_levelID.value() < right->m_levelID.value();
    }
};

struct SortByStars : Sorter {
    int getFixedDemonInt(GJGameLevel* level) {
        switch (level->m_demonDifficulty) {
            case 3: return 1;
            case 4: return 2;
            case 5: return 9;
            case 6: return 10;
            default: return 6;
        }
    }
    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) {
        int lstars = left->m_stars.value();
        int rstars = right->m_stars.value();

        if (lstars == 10 && rstars == 10) {
            int ld = getFixedDemonInt(left);
            int rd = getFixedDemonInt(right);
            if (ld != rd)
                return ld < rd;
        }

        if (lstars == rstars) {
            return getRating(left) < getRating(right);
        }

        return lstars < rstars;
    }
};

struct SortByLevelName : Sorter {
    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) {
        using namespace geode::utils::string;
        return toLower(left->m_levelName) < toLower(right->m_levelName);
    }
};

struct SortByAuthorName : Sorter {
    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) {
        using namespace geode::utils::string;
        return toLower(left->m_creatorName) < toLower(right->m_creatorName);
    }
};

Sorter* getSorter(Method method);

struct SortByRating : Sorter {


    virtual bool operator_lt(GJGameLevel* left, GJGameLevel* right) {
        auto lr = getRating(left);
        auto rr = getRating(right);
        if (lr == rr) {
            return getSorter(Method::Stars)->operator_lt(left, right);
        }
        return lr < rr;
    }
};

Sorter* getSorter(Method method) {
    static SortById id;
    static SortByLevelName name;
    static SortByStars stars;
    static SortByAuthorName author;
    static SortByRating rating;

    switch (method) {
        case Method::Stars: return &stars;
        case Method::Id: return &id;
        case Method::LevelName: return &name;
        case Method::AuthorName: return &author;
        case Method::Rating: return &rating; break;
    }
}

GJFeatureState getFeatureState(LevelRating r) {
    switch (r) {
        case LevelRating::None: return GJFeatureState::None;
        case LevelRating::StarOnly: return GJFeatureState::None;
        case LevelRating::Featured: return GJFeatureState::Featured;
        case LevelRating::Epic: return GJFeatureState::Epic;
        case LevelRating::Legendary: return GJFeatureState::Legendary;
        case LevelRating::Mythic: return GJFeatureState::Mythic;
    }
}


struct ChooseSortLayer : public geode::Popup {
    LevelListLayer* listlayer = nullptr;
    std::vector<GJGameLevel*> newOrder;
    CCMenuItemToggler* activeSorter = nullptr;
    CCNode* previewNode = nullptr;

    void sortList(Sorter* sorter) {
        if (newOrder.empty()) {
            auto& list = listlayer->m_levelList;
            auto levels = CCArrayExt<GJGameLevel*>(list->getListLevelsArray(nullptr));
            std::copy(levels.begin(), levels.end(), std::back_inserter(newOrder));
        }

        std::sort(newOrder.begin(), newOrder.end(), [&](GJGameLevel* a, GJGameLevel* b) {
            return sorter->operator_lt(a, b);
        });
        updatePreview(newOrder);
    }

    void onApply(CCObject*) {
        for (int i = 0; GJGameLevel* l : newOrder) {
            listlayer->m_levelList->reorderLevel(l->m_levelID.value(), i);
            i++;
        }

        listlayer->onRefresh(nullptr);
        onClose(nullptr);
    }

    void toggleSort(CCObject* sender) {
        auto btn = static_cast<CCMenuItemToggler*>(sender);
        auto& levellist = listlayer->m_levelList;
        if (activeSorter) {
            activeSorter->toggle(false);
            activeSorter->setClickable(true);
        }
        sortList(getSorter(static_cast<Method>(btn->getTag())));
        activeSorter = btn;
        activeSorter->setClickable(false);
    }

    void updatePreview(std::span<GJGameLevel*> levels) {
        if (!previewNode)
            return;

        auto children = CCArrayExt<CCNode*>(previewNode->getChildren());
        auto itlevel = levels.begin();
        for (auto it = children.begin(); it != children.end(); it += 2) {
            auto diffSprite = static_cast<GJDifficultySprite*>(*it);
            GJGameLevel* level = *itlevel;
            diffSprite->updateDifficultyFrame(getDiffSpriteAverage(level), GJDifficultyName::Short);
            diffSprite->updateFeatureState(getFeatureState(getRating(level)));
            auto levelName = static_cast<CCLabelBMFont*>(*(it + 1));
            levelName->setString((*itlevel)->m_levelName.c_str());
            levelName->limitLabelWidth(70, 0.35f, 0.2f);
            ++itlevel;
        }
    }

    void setupPreview(CCArrayExt<GJGameLevel*> levels) {
        if (previewNode)
            return;

        previewNode = CCNode::create();
        auto winSize = CCDirector::get()->getWinSize();
        previewNode->setAnchorPoint({0.0f, 0.0f});
        previewNode->setPosition({20.f, 30.f});
        m_mainLayer->addChild(previewNode);

        constexpr int rows = 3;
        constexpr int columns = 10;

        auto it = levels.begin();
        for (int i = 0; i < rows; i++) {
            for (int j = columns; j >= 0; j--) {
                if (it == levels.end())
                    return;
                GJGameLevel* level = *it;

                auto diffSprite =
                        GJDifficultySprite::create(getDiffSpriteAverage(level), static_cast<GJDifficultyName>(0));
                diffSprite->updateFeatureState(getFeatureState(getRating(level)));
                diffSprite->setScale(.35f);
                auto levelName = CCLabelBMFont::create(level->m_levelName.c_str(), "bigFont.fnt");
                levelName->limitLabelWidth(70, 0.35f, 0.2f);
                levelName->setAnchorPoint({0.0, 0.5f});
                diffSprite->setPosition({static_cast<float>(i * 120), j * (levelName->getContentHeight() / 2) - 5});
                levelName->setPosition(diffSprite->getPosition() + CCPoint{10, 2});
                previewNode->addChild(diffSprite);
                previewNode->addChild(levelName);

                ++it;
            }
        }
    }

    bool init(LevelListLayer* list) {
        if (!Popup::init(350, 280, "GJ_square01.png"))
            return false;

        setTitle("Choose Sorting Method");
        this->listlayer = list;

        auto sortButtonMenu = CCMenu::create();
        sortButtonMenu->setLayout(geode::RowLayout::create());

        for (int i = 0; i <= static_cast<int>(Method::Last); i++) {
            auto btnName = getMethodBtnStr(static_cast<Method>(i));
            auto first = ButtonSprite::create(btnName);
            first->setScale(0.5f);
            auto second = ButtonSprite::create(btnName);
            second->setScale(0.5f);
            second->updateBGImage("GJ_button_02.png");
            auto btn = CCMenuItemToggler::create(first, second, this, menu_selector(ChooseSortLayer::toggleSort));
            btn->setTag(i);
            sortButtonMenu->addChild(btn);
        }

        sortButtonMenu->updateLayout();

        m_mainLayer->addChild(sortButtonMenu);

        auto winSize = CCDirector::get()->getWinSize();
        sortButtonMenu->setPosition(m_title->getPosition() + CCPoint(0.f, -20.f));

        auto applySpr = ButtonSprite::create("Apply");
        applySpr->setScale(.5f);
        auto applyBtn = CCMenuItemSpriteExtra::create(applySpr, this, menu_selector(ChooseSortLayer::onApply));

        applyBtn->setPosition(sortButtonMenu->getPosition() + CCPoint{0, -20});
        m_buttonMenu->addChild(applyBtn);

        setupPreview(CCArrayExt<GJGameLevel*>(listlayer->m_levelList->getListLevelsArray(nullptr)));

        return true;
    }

    static ChooseSortLayer* create(LevelListLayer* listlayer) {
        auto ret = new ChooseSortLayer;
        if (ret->init(listlayer)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

struct MyListLayer : geode::Modify<MyListLayer, LevelListLayer> {
    void onSort(CCObject*) { ChooseSortLayer::create(this)->show(); }
    $override bool init(GJLevelList* list) {
        if (!LevelListLayer::init(list))
            return false;

        if (list->m_listType != GJLevelType::Editor)
            return true;

        if (auto node = getChildByID("right-side-menu")) {
            geode::log::info("C");
            auto spr = geode::CircleButtonSprite::createWithSpriteFrameName("GJ_sortIcon_001.png");
            spr->setScale(.8f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(MyListLayer::onSort));
            btn->setID("sort-button"_spr);

            node->addChild(btn);
            node->updateLayout();
        }
        return true;
    }
};
