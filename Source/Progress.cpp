#include "Progress.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>
#include <system_error>

namespace riot
{
namespace
{
constexpr int saveVersion = 1;
constexpr unsigned allGoals = (1u << goalsPerLevel) - 1u;

std::string_view trim(std::string_view text)
{
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t'))
        text.remove_prefix(1);
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r'))
        text.remove_suffix(1);
    return text;
}

bool readInt(std::string_view text, int& value)
{
    text = trim(text);
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && end == text.data() + text.size();
}

template <typename T, std::size_t N>
void readList(std::string_view text, std::array<T, N>& values)
{
    for (std::size_t i = 0; i < N && !text.empty(); ++i)
    {
        const auto comma = text.find(',');
        int value = 0;
        if (readInt(text.substr(0, comma), value))
            values[i] = static_cast<T>(value);
        text = comma == std::string_view::npos ? std::string_view{} : text.substr(comma + 1);
    }
}

template <typename T, std::size_t N>
std::string writeList(const std::array<T, N>& values)
{
    std::string result;
    for (std::size_t i = 0; i < N; ++i)
        result += (i ? "," : "") + std::to_string(values[i]);
    return result;
}

template <typename Def, std::size_t N>
int findId(const std::array<Def, N>& list, std::string_view id)
{
    for (std::size_t i = 0; i < N; ++i)
        if (id == list[i].id)
            return static_cast<int>(i);
    return 0;
}

std::string money(int amount) { return "$" + std::to_string(amount); }
}

int Progress::totalStars() const
{
    int total = 0;
    for (const int value : stars)
        total += value;
    return total;
}

int Progress::pointsEarned() const { return totalStars() * pointsPerStar; }

int Progress::pointsSpent() const
{
    int total = 0;
    for (int i = 0; i < upgradeCount; ++i)
        for (int tier = 0; tier < std::min(tiers[i], upgrades[i].tierCount); ++tier)
            total += upgrades[i].tiers[tier].cost;
    return total;
}

bool Progress::levelUnlocked(int index) const
{
    if (index < 0 || index >= levelCount)
        return false;
    // A level already cleared stays open even if the one before it never was (e.g. after --level).
    return index == 0 || (stars[index - 1] > 0 && defended[index - 1] != 0) || stars[index] > 0;
}

bool Progress::goalTypeMet(GoalType type) const
{
    for (int level = 0; level < levelCount; ++level)
        for (int goal = 0; goal < goalsPerLevel; ++goal)
            if (((goalsMet[level] >> goal) & 1u) && levels[level].goals[goal].type == type)
                return true;
    return false;
}

bool Progress::unlocked(const Unlock& unlock) const
{
    switch (unlock.type)
    {
    case UnlockType::Always:
        return true;
    case UnlockType::TotalStars:
        return totalStars() >= unlock.amount;
    case UnlockType::LevelCleared:
        return unlock.amount >= 1 && unlock.amount <= levelCount && stars[unlock.amount - 1] > 0;
    case UnlockType::GoalMet:
        return goalTypeMet(unlock.goal);
    }
    return false;
}

int Progress::upgradeAmount(UpgradeEffect effect) const
{
    int total = 0;
    for (int i = 0; i < upgradeCount; ++i)
        if (upgrades[i].effect == effect && tiers[i] > 0)
            total += upgrades[i].tiers[std::min(tiers[i], upgrades[i].tierCount) - 1].amount;
    return total;
}

int Progress::nextTierCost(int upgrade) const
{
    if (upgrade < 0 || upgrade >= upgradeCount || tiers[upgrade] >= upgrades[upgrade].tierCount)
        return 0;
    return upgrades[upgrade].tiers[tiers[upgrade]].cost;
}

bool Progress::canBuy(int upgrade) const
{
    const int cost = nextTierCost(upgrade);
    return cost > 0 && cost <= points();
}

bool Progress::anyAffordable() const
{
    for (int i = 0; i < upgradeCount; ++i)
        if (canBuy(i))
            return true;
    return false;
}

void Progress::record(int level, int newStars, unsigned goals)
{
    if (level < 0 || level >= levelCount)
        return;
    stars[level] = std::max(stars[level], std::clamp(newStars, 0, maxStarsPerLevel));
    goalsMet[level] |= goals & allGoals;
}

void Progress::sanitize()
{
    for (int level = 0; level < levelCount; ++level)
    {
        stars[level] = std::clamp(stars[level], 0, maxStarsPerLevel);
        goalsMet[level] = stars[level] > 0 ? goalsMet[level] & allGoals : 0u;
        defended[level] = stars[level] > 0 && defended[level] > 0 ? 1 : 0;
    }
    for (int i = 0; i < upgradeCount; ++i)
        tiers[i] = std::clamp(tiers[i], 0, upgrades[i].tierCount);
    if (pointsSpent() > pointsEarned())
        tiers = {};
    if (hat < 0 || hat >= hatCount || !unlocked(hats[hat].unlock))
        hat = 0;
    if (cone < 0 || cone >= coneCount || !unlocked(cones[cone].unlock))
        cone = 0;
}

std::string goalText(const GoalDef& goal)
{
    switch (goal.type)
    {
    case GoalType::NoBarriersLost:
        return "Lose no barriers or vans";
    case GoalType::BudgetLeft:
        return "Finish with " + money(goal.amount) + " in the bank";
    case GoalType::MaxSpend:
        return "Spend no more than " + money(goal.amount);
    case GoalType::MoodBelow:
        return "Keep the crowd mood under " + std::to_string(goal.amount) + "%";
    case GoalType::NoVans:
        return "Win without a police van";
    }
    return {};
}

std::string goalShortText(GoalType type)
{
    switch (type)
    {
    case GoalType::NoBarriersLost:
        return "NO BARRIERS LOST";
    case GoalType::BudgetLeft:
        return "MONEY LEFT OVER";
    case GoalType::MaxSpend:
        return "LOW SPENDING";
    case GoalType::MoodBelow:
        return "CALM CROWD";
    case GoalType::NoVans:
        return "NO VANS";
    }
    return {};
}

std::string unlockText(const Unlock& unlock)
{
    switch (unlock.type)
    {
    case UnlockType::Always:
        return "ALWAYS YOURS";
    case UnlockType::TotalStars:
        return "EARN " + std::to_string(unlock.amount) + " STARS";
    case UnlockType::LevelCleared:
        return "CLEAR LEVEL " + std::to_string(unlock.amount);
    case UnlockType::GoalMet:
        return "GOAL: " + goalShortText(unlock.goal);
    }
    return {};
}

std::string upgradeEffectText(const UpgradeDef& upgrade, int tier)
{
    if (tier <= 0)
        return "NOT BOUGHT YET";
    const int amount = upgrade.tiers[std::min(tier, upgrade.tierCount) - 1].amount;
    switch (upgrade.effect)
    {
    case UpgradeEffect::BarrierHealth:
        return "+" + std::to_string(amount) + "% BARRIER HEALTH";
    case UpgradeEffect::StartingBudget:
        return "+" + money(amount) + " STARTING BUDGET";
    case UpgradeEffect::RepairHealth:
        return "+" + std::to_string(amount) + " HEALTH PER REPAIR";
    case UpgradeEffect::ConeDiscount:
        return "CONES " + money(amount) + " CHEAPER";
    }
    return {};
}

std::string serialize(const Progress& progress)
{
    std::string text = "# Riot Control progress. Delete this file to start again from scratch.\n";
    text += "version=" + std::to_string(saveVersion) + "\n";
    text += "stars=" + writeList(progress.stars) + "\n";
    text += "goals=" + writeList(progress.goalsMet) + "\n";
    text += "defended=" + writeList(progress.defended) + "\n";
    for (int i = 0; i < upgradeCount; ++i)
        text += std::string("upgrade.") + upgrades[i].id + "=" + std::to_string(progress.tiers[i]) + "\n";
    text += std::string("hat=") + hats[progress.hat].id + "\n";
    text += std::string("cone=") + cones[progress.cone].id + "\n";
    return text;
}

Progress parseProgress(std::string_view text)
{
    Progress progress;
    while (!text.empty())
    {
        const auto end = text.find('\n');
        const auto line = trim(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        const auto equals = line.find('=');
        if (line.empty() || line.front() == '#' || equals == std::string_view::npos)
            continue;
        const auto key = trim(line.substr(0, equals));
        const auto value = trim(line.substr(equals + 1));
        if (key == "stars")
            readList(value, progress.stars);
        else if (key == "goals")
            readList(value, progress.goalsMet);
        else if (key == "defended")
            readList(value, progress.defended);
        else if (key == "hat")
            progress.hat = findId(hats, value);
        else if (key == "cone")
            progress.cone = findId(cones, value);
        else if (key.starts_with("upgrade."))
        {
            const auto id = key.substr(8);
            for (int i = 0; i < upgradeCount; ++i)
                if (id == upgrades[i].id)
                    readInt(value, progress.tiers[i]);
        }
    }
    progress.sanitize();
    return progress;
}

bool loadProgress(const std::filesystem::path& path, Progress& progress)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad())
        return false;
    progress = parseProgress(text);
    return true;
}

bool saveProgress(const std::filesystem::path& path, const Progress& progress)
{
    std::error_code error;
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path(), error);
    auto temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        if (!file)
            return false;
        file << serialize(progress);
        file.flush();
        if (!file)
            return false;
    }
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        return false;
    }
    return true;
}
}
