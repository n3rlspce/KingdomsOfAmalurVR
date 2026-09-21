"""Exercise the real spell commands without a game process."""
from pathlib import Path
from lupa.lua51 import LuaRuntime

lua = LuaRuntime()
lua.execute('''
    amalur_dispatch_state = {executing=true}
    ranks={}; grants=0; equips=0; equipped={}
    character_data={m_lookup_by_ability=setmetatable({}, {__index=function() return 'Sorcery' end})}
    ABILITY_ID=function(name) return name end
    ABILITY={get_ability_max_rank=function() return 3 end}
    character_data.get_current_ability_level=function(id) return ranks[id] or 0 end
    character_data.grant_actual_ability=function(id, silent)
        assert(silent == false); grants=grants+1; ranks[id]=(ranks[id] or 0)+1
    end
    character_data.setup_magic_weapon=function(id) return id..'_item' end
    character_data.init_ability_slots_from_equipment=function() end
    character_data.equip_ability_in_slot=function(item, slot)
        assert(slot>=1 and slot<=4); equipped[slot-1]=item; equips=equips+1
    end
    PLAYER={get_equipped_object_from_equip_type_and_slot=function(kind,slot)
        assert(kind=='Magic'); return equipped[slot]
    end}
''')
lua.execute(Path(__file__).with_name('amalur_dev.lua').read_text())
lua.execute('''
    assert(grants==0 and equips==0)
    amalur_dispatch_state.executing=false
    assert(not pcall(amalur_dev.max_sorcery))
    assert(not pcall(amalur_dev.equip_spell_test_set))
    amalur_dispatch_state.executing=true
    assert(not pcall(amalur_dev.equip_spell_test_set) and equips==0)
    local maximum=ABILITY.get_ability_max_rank
    ABILITY.get_ability_max_rank=function(id) if id=='Sorcery_Meteor' then return 1000 end return 3 end
    assert(not pcall(amalur_dev.max_sorcery) and grants==0)
    ABILITY.get_ability_max_rank=maximum
    amalur_dev.max_sorcery(); assert(grants==75)
    amalur_dev.max_sorcery(); assert(grants==75)
    amalur_dev.equip_spell_test_set(); assert(equips==4)
    assert(equipped[0]=='Sorcery_StormBolt_item' and equipped[3]=='Sorcery_Meteor_item')
    ranks.Sorcery_StormBolt=0
    character_data.grant_actual_ability=function() grants=grants+1 end
    assert(not pcall(amalur_dev.max_sorcery) and grants==76)
    PLAYER.get_equipped_object_from_equip_type_and_slot=function() return 'wrong' end
    ranks.Sorcery_StormBolt=3
    assert(not pcall(amalur_dev.equip_spell_test_set) and equips==5)
''')
print('PASS: dispatch-only spell cheats, preflight before mutation, idempotent ranks, exact slots, partial failure stops.')
