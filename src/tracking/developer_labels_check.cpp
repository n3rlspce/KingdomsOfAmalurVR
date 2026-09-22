#include "developer_commands.hpp"
#include <cassert>
#include <cwchar>
#include <iostream>

int main(){
    using namespace amalur::developer;
    // Construct exactly the string the renderer uses for every command row.
    for(int row=0;row<panelRows;++row){
        const std::wstring label(panelLabel(row));
        assert(!label.empty()&&label!=L"Unknown developer action");
        assert(!command(panelAction(row,0)).empty());
    }
    for(int row=2;row<rows;++row)
        assert(std::wcscmp(panelLabel(row),weapons[row-2].label)==0);
    assert(std::wstring(panelLabel(rows+2))==L"Max Sorcery (unlock all spells)");
    assert(std::wstring(panelLabel(rows+3))==L"Equip spell test set (slots 1-4)");
    // Combined panels append diagnostics; missing labels must stay bounded.
    for(int row=panelRows;row<panelRows+32;++row)
        assert(std::wstring(panelLabel(row))==L"Unknown developer action");
    assert(std::wstring(panelLabel(-1))==L"Unknown developer action");
    std::cout<<"All developer row labels and action mappings PASS\n";
}
