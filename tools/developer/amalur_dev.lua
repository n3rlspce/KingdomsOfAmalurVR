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
    local read = require_function(ACTOR and ACTOR.is_unkillable, 'ACTOR.is_unkillable')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    set(player, true)
    local enabled = read(player)
    if enabled ~= true and enabled ~= 1 then fail('player unkillable flag did not enable') end
    local health = ''
    if type(ACTOR.get_current_health) == 'function' and type(ACTOR.get_max_health) == 'function' then
        health = '; HP ' .. tostring(ACTOR.get_current_health(player)) .. '/' .. tostring(ACTOR.get_max_health(player))
    end
    return 'Death protection verified ON' .. health .. '. Damage and stagger can still occur; re-enable after loading a save.'
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

function dev.boost_health(target)
    require_dispatch()
    target = integer(target or 10000, 1000, 100000, 'target HP')
    local player = require_function(get_player, 'get_player')()
    if player == nil or player == false or player == 0 then fail('load a game first') end
    local maximum = require_function(ACTOR and ACTOR.get_max_health, 'ACTOR.get_max_health')
    local current = require_function(ACTOR.get_current_health, 'ACTOR.get_current_health')
    local heal = require_function(ACTOR.modify_health, 'ACTOR.modify_health')
    local attribute = require_function(PLAYER and PLAYER.get_unmodified_attribute, 'PLAYER.get_unmodified_attribute')
    local add = require_function(PLAYER.increment_attribute, 'PLAYER.increment_attribute')
    local before = maximum(player)
    if type(before) ~= 'number' or before <= 0 or before ~= before then fail('invalid maximum HP') end
    local might = integer(attribute('Might'), 0, 100000, 'Might')
    local added = 0
    if before < target then
        -- Measure this character's actual scaling; do not assume a level formula.
        add('Might', 1)
        added = 1
        local per_point = maximum(player) - before
        if type(per_point) ~= 'number' or per_point ~= per_point or per_point <= 0 then
            fail('Might changed by 1 but health scaling unavailable; stopped without further grants')
        end
        local missing = (target - maximum(player)) / per_point
        if missing > 0 then
            local points = missing - missing % 1
            if points < missing then points = points + 1 end
            points = integer(points, 1, 10000, 'extra Might points')
            add('Might', points)
            added = added + points
        end
    end
    local after = maximum(player)
    if type(after) ~= 'number' or after < target then fail('maximum HP target not reached; no retry') end
    local hp = current(player)
    if type(hp) ~= 'number' or hp ~= hp or hp < 0 then fail('invalid current HP') end
    if hp < after then heal(player, after - hp) end
    local result = current(player)
    if type(result) ~= 'number' or result < after - 1 then fail('maximum HP boosted but refill not confirmed') end
    return 'HP verified ' .. tostring(result) .. '/' .. tostring(after) ..
        '; Might ' .. tostring(might) .. ' -> ' .. tostring(attribute('Might')) ..
        ' (' .. added .. ' added)'
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

local function max_abilities(names)
    require_dispatch()
    local resolve = require_function(ABILITY_ID, 'ABILITY_ID')
    local data = character_data
    local grant = require_function(data and data.grant_actual_ability, 'character_data.grant_actual_ability')
    local rank = require_function(data.get_current_ability_level, 'character_data.get_current_ability_level')
    local maximum = require_function(ABILITY and ABILITY.get_ability_max_rank, 'ABILITY.get_ability_max_rank')
    local plan = {}
    -- Validate every ID/rank before the first mutation.
    for _, name in ipairs(names) do
        local id = resolve(name)
        if not id or id == 0 or not data.m_lookup_by_ability or not data.m_lookup_by_ability[id] then
            fail('unknown ability: ' .. name)
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
    return added
end

function dev.max_sorcery()
    local names = {}
    for _, name in ipairs(sorcery) do names[#names + 1] = 'Sorcery_' .. name end
    local added = max_abilities(names)
    return 'Sorcery maxed: 25 abilities; ' .. added .. ' ranks added. Assign spells in Abilities or use the spell test set.'
end

-- Exact IDs from the shipped character_data ability tree. Limit this cheat to
-- weapon masteries/move chains and the bow's charged/projectile upgrades.
local weapon_moves = {
    'Might_LongswordMastery','Might_GreatswordMastery','Might_HammerMastery',
    'Might_BrutalWeaponry01','Might_BrutalWeaponry02','Might_BrutalWeaponry03','Might_BrutalWeaponry04',
    'Finesse_DaggerMastery','Finesse_FaebladeMastery','Finesse_LongbowMastery',
    'Finesse_PreciseWeaponry01','Finesse_PreciseWeaponry02','Finesse_PreciseWeaponry03','Finesse_PreciseWeaponry04',
    'Finesse_Drawpower','Finesse_ArrowStorm','Finesse_BarbedArrows','Finesse_Scattershot',
    'Sorcery_StaffMastery','Sorcery_SceptreMastery','Sorcery_ChakramMastery',
    'Sorcery_ArcaneWeaponry01','Sorcery_ArcaneWeaponry02','Sorcery_ArcaneWeaponry03','Sorcery_ArcaneWeaponry04'
}
function dev.unlock_weapon_moves()
    local added = max_abilities(weapon_moves)
    return 'Weapon moves unlocked: 25 weapon abilities; ' .. added .. ' ranks added. All 9 weapon families.'
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
