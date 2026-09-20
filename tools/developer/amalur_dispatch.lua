-- Installed with a framework script trigger after minimap_win loads.
-- Commands execute from the game's update callback, never the console thread.
local host = minimap_win
if type(host) ~= 'table' or type(host.on_update_event) ~= 'function' then
    print('AMALUR_DISPATCH_ERROR|missing minimap update callback')
    return
end
local state = _G.amalur_dispatch_state
if not state then
    state = {seen = {}, frames = 0}
    _G.amalur_dispatch_state = state
end
if host.on_update_event == state.wrapper then return end
local original = host.on_update_event

local function poll()
    state.frames = state.frames + 1
    if state.frames % 15 ~= 0 then return end
    local read = loadfile('.\\mods\\amalur_request.lua')
    if not read then return end
    local valid, request = pcall(read)
    if not valid or type(request) ~= 'table' or type(request.nonce) ~= 'string' or
       #request.nonce > 64 or not string.match(request.nonce, '^[%w_]+$') then return end
    if state.seen[request.nonce] then return end
    -- Consume before any engine call. Reloads and repeated frames cannot retry it.
    state.seen[request.nonce] = true
    local ok, result = pcall(function()
        if request.connect ~= true and
           (state.session == nil or request.session ~= state.session) then
            error('stale session; connect again')
        end
        local module, message = loadfile('.\\mods\\amalur_dev.lua')
        assert(module, message)
        module()
        if request.connect == true then
            local result = amalur_dev.probe()
            assert(GAME and type(GAME.is_game_paused) == 'function', 'pause API unavailable')
            state.session = request.nonce
            print('AMALUR_DEV_SESSION|' .. state.session .. '|')
            return result
        end
        assert(GAME and type(GAME.is_game_paused) == 'function', 'pause API unavailable')
        local paused = GAME.is_game_paused()
        assert(paused == false or paused == 0, 'close inventory/menus before commands')
        assert(type(request.run) == 'function', 'invalid request')
        state.executing = true
        local success, value = pcall(request.run)
        state.executing = false
        if not success then error(value) end
        return value
    end)
    print('AMALUR_DEV_' .. request.nonce .. '|' ..
          (ok and 'OK|' or 'ERROR|') .. tostring(result) .. '|END')
end

state.wrapper = function(...)
    original(...)
    poll()
end
host.on_update_event = state.wrapper
print('AMALUR_DISPATCH_READY|game update callback installed')
