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

-- One actor per call. Distance is in game units, not meters.
function dev.spawn(name, distance)
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
        if type(n) ~= 'number' or n ~= n or n == math.huge or n == -math.huge then
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
function dev.give_and_equip(name, slot)
    slot = integer(slot, 0, 1, 'weapon slot')
    local find = require_function(PLAYER and PLAYER.get_item_index, 'PLAYER.get_item_index')
    local equip = require_function(PLAYER and PLAYER.equip, 'PLAYER.equip')
    local id = simtype(name)
    dev.give(name, 1)
    local item = find(id)
    if item == nil or item == false or item == -1 then
        fail('item granted but not available to equip; do not repeat the grant')
    end
    equip(item, slot)
    return 'grant/equip submitted: ' .. name .. ' slot ' .. slot
end
