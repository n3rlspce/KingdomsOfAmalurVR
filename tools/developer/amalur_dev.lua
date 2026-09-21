-- Explicit developer commands for a loaded game with the Re-Reckoning Lua framework.
-- Loading this file only defines amalur_dev. It never grants or spawns anything.
amalur_dev = {}
local dev = amalur_dev

local function fail(message)
    error('amalur_dev: ' .. message, 3)
end

local function require_function(value, name)
    if type(value) ~= 'function' then fail(name .. ' is unavailable') end
    return value
end

local function require_dispatch()
    if not (_G.amalur_dispatch_state and _G.amalur_dispatch_state.executing == true) then
        fail('use the game update dispatcher; direct console mutations are disabled')
    end
    -- Discard the earlier experimental Lua requirement overrides on hot reload.
    if _G.amalur_dispatch_state.levelOverrides then
        _G.amalur_dispatch_state.levelOverrides = {}
    end
end

-- Same native setter and arguments as the game's Enable Invincibility cheat.
function dev.enable_invincibility()
    require_dispatch()
    local set = require_function(ACTOR and ACTOR.set_unkillable, 'ACTOR.set_unkillable')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    set(player, true)
    return 'invincibility setter applied to player'
end

local function equip_verified(item, slot)
    local equip = require_function(PLAYER and PLAYER.equip, 'PLAYER.equip')
    local current = require_function(PLAYER and PLAYER.get_equipped_object_from_equip_type_and_slot,
                                    'PLAYER.get_equipped_object_from_equip_type_and_slot')
    _G.amalur_dispatch_state.lastEquip = {item = item, slot = slot}
    equip(item, slot)
    local observed = current('Weapon', slot)
    if observed ~= item then
        fail('weapon not confirmed: requested ' .. tostring(item) .. ' (' .. type(item) ..
             '), observed ' .. tostring(observed) .. ' (' .. type(observed) .. '); no repeat grant')
    end
end

function dev.verify_last_equip()
    require_dispatch()
    local last = _G.amalur_dispatch_state.lastEquip
    if not last then fail('no equipment request in this session') end
    local current = require_function(PLAYER and PLAYER.get_equipped_object_from_equip_type_and_slot,
                                    'PLAYER.get_equipped_object_from_equip_type_and_slot')
    if current('Weapon', last.slot) ~= last.item then fail('requested weapon is not in the target slot') end
    return 'verified weapon in slot ' .. last.slot .. ': ' .. tostring(last.item)
end

local function integer(value, low, high, name)
    if type(value) ~= 'number' or value ~= value or
       value < low or value > high or value % 1 ~= 0 then
        fail(name .. ' must be an integer from ' .. low .. ' to ' .. high)
    end
    return value
end

local function simtype(name)
    if type(name) ~= 'string' or #name > 128 or
       not string.match(name, '^[%w_]+$') then
        fail('use an internal simtype name containing letters, digits or underscores')
    end
    local resolve = require_function(SIMTYPE_ID, 'SIMTYPE_ID')
    local ok, id = pcall(resolve, name)
    if not ok or id == nil or id == false or id == 0 then
        fail('simtype could not be resolved: ' .. name)
    end
    return id
end

-- Passive capability check: never invoke engine or UI functions from this probe.
function dev.probe()
    require_function(get_player, 'get_player')
    require_function(SIMTYPE_ID, 'SIMTYPE_ID')
    require_function(PROTO and PROTO.create_actor, 'PROTO.create_actor')
    require_function(ACTOR and ACTOR.get_angle, 'ACTOR.get_angle')
    require_function(ACTOR and ACTOR.get_point_near_object, 'ACTOR.get_point_near_object')
    require_function(PLAYER and PLAYER.cheat_add_item, 'PLAYER.cheat_add_item')
    require_function(PLAYER and PLAYER.get_item_index, 'PLAYER.get_item_index')
    require_function(PLAYER and PLAYER.equip, 'PLAYER.equip')
    return 'developer APIs present; gameplay operations untested'
end

-- Explicit dev-save setup. Uses the same setter as the game's equipment cheats.
function dev.prepare_test_character()
    require_dispatch()
    local get_level = require_function(PLAYER and PLAYER.get_level, 'PLAYER.get_level')
    local set_level = require_function(PLAYER and PLAYER.set_level, 'PLAYER.set_level')
    local before = get_level()
    if type(before) ~= 'number' then fail('player level unavailable') end
    if before < 40 then set_level(40) end
    local after = get_level()
    if type(after) ~= 'number' or after < 40 then fail('developer level change did not apply') end
    return 'developer character level: ' .. tostring(after)
end

-- Ability IDs from character_data's Sorcery tree. Grant one rank at a time,
-- using the same helper as the game's own max-abilities cheat (cheats #288).
local sorcery = {
    'StaffMastery','SceptreMastery','ConservativeCasting','ChakramMastery',
    'ArcaneWeaponry01','StormBolt','HealingSurge','IceBarrage','MarkOfFlame',
    'SphereOfProtection','Summon','ArcaneWeaponry02','ChainLightning','Tempest',
    'ElementalRage','SummonUpgrade1','SummonUpgrade2','ArcaneWeaponry03',
    'ArcaneWeaponry04','Frostshackle','WintersEmbrace','Smolder','Meteor',
    'SphereOfReprisal','SphereOfRetribution'
}

function dev.max_sorcery()
    require_dispatch()
    local resolve = require_function(ABILITY_ID, 'ABILITY_ID')
    local data = character_data
    local grant = require_function(data and data.grant_actual_ability, 'character_data.grant_actual_ability')
    local rank = require_function(data.get_current_ability_level, 'character_data.get_current_ability_level')
    local maximum = require_function(ABILITY and ABILITY.get_ability_max_rank, 'ABILITY.get_ability_max_rank')
    local plan = {}
    -- Validate every ID/rank before the first mutation.
    for _, name in ipairs(sorcery) do
        local id = resolve('Sorcery_' .. name)
        if not id or id == 0 or not data.m_lookup_by_ability or not data.m_lookup_by_ability[id] then
            fail('unknown Sorcery ability: ' .. name)
        end
        local target = integer(maximum(id), 1, 20, 'maximum ability rank')
        local current = integer(rank(id), 0, 30, 'current ability rank')
        plan[#plan + 1] = {id=id, name=name, target=target, current=current}
    end
    local added = 0
    for _, ability in ipairs(plan) do
        for level = ability.current + 1, ability.target do
            grant(ability.id, false)
            if rank(ability.id) < level then
                fail('rank did not advance for ' .. ability.name .. '; partial grant, not retried')
            end
            added = added + 1
        end
    end
    return 'Sorcery maxed: 25 abilities; ' .. added .. ' ranks added. Assign spells in Abilities or use the spell test set.'
end

function dev.equip_spell_test_set()
    require_dispatch()
    local resolve = require_function(ABILITY_ID, 'ABILITY_ID')
    local data = character_data
    local rank = require_function(data and data.get_current_ability_level, 'character_data.get_current_ability_level')
    local setup = require_function(data.setup_magic_weapon, 'character_data.setup_magic_weapon')
    local init = require_function(data.init_ability_slots_from_equipment, 'character_data.init_ability_slots_from_equipment')
    local equip = require_function(data.equip_ability_in_slot, 'character_data.equip_ability_in_slot')
    local current = require_function(PLAYER and PLAYER.get_equipped_object_from_equip_type_and_slot, 'PLAYER.get_equipped_object_from_equip_type_and_slot')
    local ids = {}
    local names = {'StormBolt','IceBarrage','HealingSurge','Meteor'}
    for i, name in ipairs(names) do
        ids[i] = resolve('Sorcery_' .. name)
        if not ids[i] or ids[i] == 0 or rank(ids[i]) < 1 then fail('use Max Sorcery first: ' .. name .. ' is locked') end
    end
    init()
    for slot, id in ipairs(ids) do
        local item = setup(id)
        if item == nil or item == false or item == -1 then fail('spell item unavailable; partial setup, not retried') end
        equip(item, slot) -- character_data uses 1-based slots; PLAYER uses 0-based.
        if current('Magic', slot - 1) ~= item then fail('spell slot not confirmed; not retried') end
    end
    return 'Spell slots 1-4: Storm Bolt / Ice Barrage / Healing Surge / Meteor'
end

-- One actor per call. Distance is in game units, not meters.
function dev.spawn(name, distance)
    require_dispatch()
    if distance == nil then distance = 500 end
    distance = integer(distance, 100, 2000, 'distance')
    local create = require_function(PROTO and PROTO.create_actor, 'PROTO.create_actor')
    local get_angle = require_function(ACTOR and ACTOR.get_angle, 'ACTOR.get_angle')
    local near = require_function(ACTOR and ACTOR.get_point_near_object,
                                  'ACTOR.get_point_near_object')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    local id = simtype(name)
    local pos = near(player, get_angle(player), distance)
    if type(pos) ~= 'table' then fail('no spawn position returned') end
    for i = 1, 3 do
        local n = pos[i]
        if type(n) ~= 'number' or n ~= n or (n - n) ~= 0 then
            fail('invalid spawn position')
        end
    end
    create(id, pos[1], pos[2], pos[3], player)
    return 'spawn submitted: ' .. name
end

function dev.wolf(distance)
    return dev.spawn('wolf_forest', distance)
end

-- Any resolvable item simtype; weapon example below.
function dev.give(name, quantity)
    require_dispatch()
    if quantity == nil then quantity = 1 end
    quantity = integer(quantity, 1, 20, 'quantity')
    local give = require_function(PLAYER and PLAYER.cheat_add_item, 'PLAYER.cheat_add_item')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    local id = simtype(name)
    give(id, quantity)
    return 'grant submitted: ' .. name
end

function dev.sword()
    return dev.give('sword2h_unique12f', 1)
end

-- Native inventory slots: 0 primary, 1 secondary. These are not VR hands.
-- Equip an existing item without granting another copy.
function dev.equip_existing(name, slot)
    require_dispatch()
    slot = integer(slot, 0, 1, 'weapon slot')
    local find = require_function(PLAYER and PLAYER.get_item_index, 'PLAYER.get_item_index')
    local equip = require_function(PLAYER and PLAYER.equip, 'PLAYER.equip')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    local item = find(simtype(name))
    if item == nil or item == false or item == -1 then
        fail('item is not in inventory; nothing granted or equipped')
    end
    equip_verified(item, slot)
    return 'equipped: ' .. name .. ' slot ' .. slot
end

function dev.give_and_equip(name, slot)
    require_dispatch()
    slot = integer(slot, 0, 1, 'weapon slot')
    local find = require_function(PLAYER and PLAYER.get_item_index, 'PLAYER.get_item_index')
    local equip = require_function(PLAYER and PLAYER.equip, 'PLAYER.equip')
    local id = simtype(name)
    dev.give(name, 1)
    local item = find(id)
    if item == nil or item == false or item == -1 then
        fail('item granted but not available to equip; do not repeat the grant')
    end
    equip_verified(item, slot)
    return 'granted and equipped: ' .. name .. ' slot ' .. slot
end
