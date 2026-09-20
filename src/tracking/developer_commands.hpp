#pragma once
#include <array>
#include <string>

namespace amalur::developer {
struct Weapon { const wchar_t* label; const char* simtype; };
// Low-tier representatives from the public Re-Reckoning simtype catalog.
inline constexpr std::array<Weapon,9> weapons{{
    {L"Longsword", "sword1h_common01a"},
    {L"Greatsword", "sword2h_common01a"},
    {L"Hammer", "greathammer_common01a"},
    {L"Daggers", "daggers_common01a"},
    {L"Faeblades", "mirrorblades_common01a"},
    {L"Longbow", "longbow_common01a"},
    {L"Staff", "staff_common01a_fire"},
    {L"Sceptre", "sceptre_merchant_common01a"},
    {L"Chakrams", "chakrams_common01a_fire"}
}};
inline constexpr int rows=11; // connect, wolf, nine weapons
inline constexpr int actions=rows*3; // inventory, primary, secondary
inline std::string command(int row) {
    if(row<0||row>=actions)return {};
    int destination=row/rows;row%=rows;
    if(destination&&row<2)return {};
    if(row==0)return "amalur_dev.probe()";
    if(row==1)return "amalur_dev.wolf()";
    if(row<2||row>=rows)return {};
    if(destination)return std::string("amalur_dev.give_and_equip('")+weapons[row-2].simtype+"',"+std::to_string(destination-1)+")";
    return std::string("amalur_dev.give('")+weapons[row-2].simtype+"',1)";
}
}
