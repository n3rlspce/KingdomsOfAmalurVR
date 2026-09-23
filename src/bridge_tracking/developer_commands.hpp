#pragma once
#include <array>
#include <string>

namespace amalur::developer {
struct Weapon { const wchar_t* label; const char* simtype; };
// Low-tier representatives from the public Re-Reckoning simtype catalog.
inline constexpr std::array<Weapon,10> weapons{{
    {L"Longsword", "sword1h_common01a"},
    {L"Greatsword", "sword2h_common01a"},
    {L"Hammer", "greathammer_common01a"},
    {L"Daggers", "daggers_common01a"},
    {L"Faeblades", "mirrorblades_common01a"},
    {L"Longbow", "longbow_common01a"},
    {L"Staff", "staff_common01a_fire"},
    {L"Sceptre", "sceptre_merchant_common01a"},
    {L"Chakrams", "chakrams_common01a_fire"},
    {L"Unique greatsword", "sword2h_unique12f"}
}};
inline constexpr int rows=2+static_cast<int>(weapons.size());
inline constexpr int destinations=5; // give; give/equip primary/secondary; equip existing primary/secondary
inline constexpr int prepareCharacterAction=rows*destinations;
inline constexpr int verifyEquipAction=prepareCharacterAction+1;
inline constexpr int invincibilityAction=verifyEquipAction+1;
inline constexpr int sorceryAction=invincibilityAction+1;
inline constexpr int spellSetAction=sorceryAction+1;
inline constexpr int weaponMovesAction=spellSetAction+1;
inline constexpr int weakenNearbyAction=weaponMovesAction+1;
inline constexpr int actions=weakenNearbyAction+1;
inline constexpr int panelRows=rows+6;
// Keep labels beside the row/action mapping. Extra diagnostic rows must never
// fall through to indexing the weapon array.
inline const wchar_t* panelLabel(int row){
    if(row==0)return L"Reconnect (connection is automatic)";
    if(row==1)return L"Spawn one wolf";
    if(row>=2&&row<rows)return weapons[row-2].label;
    if(row==rows)return L"Dev character: level 40";
    if(row==rows+1)return L"Set health to ~10,000";
    if(row==rows+2)return L"Max Sorcery (unlock all spells)";
    if(row==rows+3)return L"Equip spell test set (slots 1-4)";
    if(row==rows+4)return L"Unlock weapon moves (all types)";
    if(row==rows+5)return L"Weaken nearby enemies to 1 HP (20 m)";
    return L"Unknown developer action";
}
inline int panelAction(int row,int destination){
    if(row==rows)return prepareCharacterAction;
    if(row==rows+1)return invincibilityAction;
    if(row==rows+2)return sorceryAction;
    if(row==rows+3)return spellSetAction;
    if(row==rows+4)return weaponMovesAction;
    if(row==rows+5)return weakenNearbyAction;
    if(row<0||row>=rows||destination<0||destination>=destinations)return -1;
    return row+(row>=2?destination*rows:0);
}
inline std::string command(int row) {
    if(row<0||row>=actions)return {};
    if(row==prepareCharacterAction)return "amalur_dev.prepare_test_character()";
    if(row==verifyEquipAction)return "amalur_dev.verify_last_equip()";
    if(row==invincibilityAction)return "amalur_dev.boost_health(10000)";
    if(row==sorceryAction)return "amalur_dev.max_sorcery()";
    if(row==spellSetAction)return "amalur_dev.equip_spell_test_set()";
    if(row==weaponMovesAction)return "amalur_dev.unlock_weapon_moves()";
    if(row==weakenNearbyAction)return "amalur_dev.weaken_nearby()";
    int destination=row/rows;row%=rows;
    if(destination&&row<2)return {};
    if(row==0)return "amalur_dev.probe()";
    if(row==1)return "amalur_dev.wolf()";
    if(row<2||row>=rows)return {};
    if(destination>=3)return std::string("amalur_dev.equip_existing('")+weapons[row-2].simtype+"',"+std::to_string(destination-3)+")";
    if(destination)return std::string("amalur_dev.give_and_equip('")+weapons[row-2].simtype+"',"+std::to_string(destination-1)+")";
    return std::string("amalur_dev.give('")+weapons[row-2].simtype+"',1)";
}
}
