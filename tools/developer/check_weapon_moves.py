"""Execute the weapon-only grant command under Lua 5.1 without a game."""
from pathlib import Path
from lupa.lua51 import LuaRuntime

lua = LuaRuntime()
lua.execute('''
    amalur_dispatch_state={executing=true}
    ranks={}; grants=0; ids={}
    ABILITY_ID=function(name) ids[name]=true; return name end
    character_data={m_lookup_by_ability=setmetatable({}, {__index=function() return true end})}
    character_data.get_current_ability_level=function(id) return ranks[id] or 0 end
    character_data.grant_actual_ability=function(id, silent)
        assert(silent==false); grants=grants+1; ranks[id]=(ranks[id] or 0)+1
    end
    ABILITY={get_ability_max_rank=function() return 3 end}
''')
lua.execute(Path(__file__).with_name('amalur_dev.lua').read_text())
lua.execute('''
    assert(grants==0)
    amalur_dispatch_state.executing=false
    assert(not pcall(amalur_dev.unlock_weapon_moves) and grants==0)
    amalur_dispatch_state.executing=true
    character_data.m_lookup_by_ability.Sorcery_ArcaneWeaponry04=false
    assert(not pcall(amalur_dev.unlock_weapon_moves) and grants==0)
    character_data.m_lookup_by_ability.Sorcery_ArcaneWeaponry04=true
    amalur_dev.unlock_weapon_moves(); assert(grants==75)
    local n=0; for id in pairs(ids) do n=n+1; assert(ranks[id]==3) end
    assert(n==25)
    for _, tree in ipairs({'Might_BrutalWeaponry','Finesse_PreciseWeaponry','Sorcery_ArcaneWeaponry'}) do
        for i=1,4 do assert(ids[tree..'0'..i]) end
    end
    assert(ids.Finesse_Drawpower and ids.Finesse_ArrowStorm and ids.Finesse_Scattershot)
    assert(not ids.Sorcery_Meteor and not ids.Might_HardyConstitution and not ids.Finesse_SmokeBomb)
    amalur_dev.unlock_weapon_moves(); assert(grants==75)
    ranks.Might_LongswordMastery=0
    character_data.grant_actual_ability=function() grants=grants+1 end
    assert(not pcall(amalur_dev.unlock_weapon_moves) and grants==76)
''')
print('PASS: weapon-only 25-ID scope, dispatch/preflight, complete chains, idempotence and failure stop.')
