-- Fill a missing native NPC look-at target during visible conversations.
-- The stock noncombat_behavior uses this exact controller/target-node pair.
-- Existing targets, scripted head-tracking disables, and NPC body facing win.
local host = _G.conversation_menu
if type(host) ~= 'table' or type(host.on_update_event) ~= 'function' then return end
local state = _G.amalur_vr_dialogue_gaze_state
if not state then
    state = {hosts = setmetatable({}, {__mode='k'})}
    _G.amalur_vr_dialogue_gaze_state = state
end
local previous = state.hosts[host]
if previous then return end
local entry = {}
state.hosts[host] = entry
local original = host.on_update_event
local function flag(v) return v == true or v == 1 end
local function valid(v) return v ~= nil and v ~= false and v ~= 0 end
local function note(npc,action)
    if entry.observedNpc~=npc then
        entry.observedNpc=npc
        print('AMALUR_VR_GAZE|' .. action)
    end
end
local function gaze()
    if not flag(host.m_inited) or not valid(host.m_window) or
       not flag(WINDOW.is_visible(host.m_window)) then entry.observedNpc=nil;return end
    local npc = ACTOR.get_active_dialog_target_npc()
    local player = get_player()
    if not valid(npc) or not valid(player) or npc == player then return end
    if flag(ACTOR.has_actor_state(npc, ACTOR_STATE_ID('NPC_DisableHeadTracking'))) then
        note(npc,'scripted head tracking disabled');return
    end
    local hasTarget = ACTOR.look_at_has_target(npc)
    if hasTarget ~= false and hasTarget ~= 0 then note(npc,'existing or unknown native target preserved');return end
    -- Native noncombat_behavior (asset 109552): actor, look controller,
    -- target actor, target attachment. No teleport or actor rotation.
    ACTOR.look_at_set_target_object(npc, 1985713, player, 701554)
    note(npc,'missing target filled with native player look-at')
end
local function pack(...) return {n=select('#', ...), ...} end
entry.wrapper = function(...)
    local results = pack(original(...))
    if not entry.failed then
        local ok, err = pcall(gaze)
        if not ok then
            entry.failed = true
            print('AMALUR_VR_GAZE_ERROR|' .. tostring(err))
        end
    end
    return unpack(results, 1, results.n)
end
host.on_update_event = entry.wrapper
