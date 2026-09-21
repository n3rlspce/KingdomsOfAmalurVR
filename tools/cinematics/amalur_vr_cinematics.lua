-- Re-Reckoning framework script triggers run on the game thread.
-- Hide the native letterbox subtree, including its textured fuzzy edges.
-- Do not filter shared shaders or change dialogue/cinematic lifecycle calls.
local state = _G.amalur_vr_cinematics_state
if not state then
    state = {hosts = setmetatable({}, {__mode = 'k'})}
    _G.amalur_vr_cinematics_state = state
end

local function pack(...)
    return {n = select('#', ...), ...}
end

local function hide_letterbox(host)
    if (host.m_inited ~= true and host.m_inited ~= 1) or
       not host.m_window or host.m_window == 0 then return end
    -- Resolve from the current window every time: conversation windows are
    -- destroyed and recreated. Never retain an engine handle across callbacks.
    if type(WINDOW) ~= 'table' or type(WINDOW.find_window) ~= 'function' or
       type(WINDOW.is_visible) ~= 'function' or type(WINDOW.set_visible) ~= 'function' then
        error('native WINDOW API unavailable')
    end
    local box = WINDOW.find_window(host.m_window, 'letterbox')
    if box == nil or box == false or box == 0 then return end
    local visible = WINDOW.is_visible(box)
    if visible == true or visible == 1 then WINDOW.set_visible(box, false) end
end

for _, name in ipairs({'conversation_menu', 'cinematic_paused_win'}) do
    local host = _G[name]
    if type(host) == 'table' then
        local entry = state.hosts[host]
        if not entry then
            entry = {wrappers = {}}
            state.hosts[host] = entry
        end
        local function after_native()
            if entry.failed then return end
            local ok, message = pcall(hide_letterbox, host)
            if not ok then
                -- Cosmetic failure must never prevent native input, skipping,
                -- subtitles, a scene ending, or a conversation progressing.
                entry.failed = true
                print('AMALUR_VR_CINEMATICS_ERROR|' .. name .. '|' .. tostring(message))
            end
        end
        for _, callback in ipairs({'init', 'on_window_event', 'on_update_event',
                                   'on_position_event', 'show_letterbox'}) do
            local original = host[callback]
            -- A different mod may wrap ours later. Do not wrap that chain again
            -- when the other host's trigger fires or this script is reloaded.
            if type(original) == 'function' and not entry.wrappers[callback] then
                local wrapper = function(...)
                    local results = pack(original(...))
                    after_native()
                    return unpack(results, 1, results.n)
                end
                entry.wrappers[callback] = wrapper
                host[callback] = wrapper
            end
        end
        -- Trigger installation only wraps functions. All native UI reads/writes
        -- happen from those callbacks after the original has initialized them.
    end
end
