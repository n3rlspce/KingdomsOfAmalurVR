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
inline constexpr int actions=invincibilityAction+1;
inline std::string command(int row) {
    if(row<0||row>=actions)return {};
    if(row==prepareCharacterAction)return "amalur_dev.prepare_test_character()";
    if(row==verifyEquipAction)return "amalur_dev.verify_last_equip()";
    if(row==invincibilityAction)return "amalur_dev.enable_invincibility()";
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
