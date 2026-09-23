-- Lua178: set_up_window_components(window, actor) owns bar.sync_icon.
-- Native lookup: find_static_type(window, 0x0016B440, 0x00F3A55E, -1, true).
-- get_width/get_height/resize signatures also occur in its bar-helper scale().
-- Store state on the native bar table: a recreated window receives a fresh table.
local host = target_win_manager
if type(host) ~= 'table' or type(host.set_up_window_components) ~= 'function' then
    print('AMALUR_FINISHER_ICON_ERROR|target module unavailable')
    return
end
local state = rawget(host, '__amalur_half_sync_icon')
if state and host.set_up_window_components == state.wrapper then return end
state = {}
local original = host.set_up_window_components
local function resize_icon(actor)
    local bar = host.m_bars_by_target and host.m_bars_by_target[actor]
    if type(bar) ~= 'table' then return end
    local icon = bar.sync_icon
    if icon == nil or icon == false or icon == 0 then return end
    if bar.__amalur_half_sync_icon == icon then return end
    if type(WINDOW) ~= 'table' or type(WINDOW.get_width) ~= 'function' or
       type(WINDOW.get_height) ~= 'function' or type(WINDOW.resize) ~= 'function' then
        error('native WINDOW size API unavailable')
    end
    local width, height = WINDOW.get_width(icon), WINDOW.get_height(icon)
    if type(width) ~= 'number' or type(height) ~= 'number' or
       width <= 0 or height <= 0 or width ~= width or height ~= height or
       width == math.huge or height == math.huge then return end
    WINDOW.resize(icon, width / 2, height / 2)
    bar.__amalur_half_sync_icon = icon
    if not state.reported then
        state.reported = true
        print('AMALUR_FINISHER_ICON_RESIZED|half-width-height')
    end
end
state.wrapper = function(window, actor, ...)
    -- The original has no return values (Lua178 RETURN B=1).
    original(window, actor, ...)
    local ok, message = pcall(resize_icon, actor)
    if not ok and not state.errorReported then
        state.errorReported = true
        print('AMALUR_FINISHER_ICON_ERROR|' .. tostring(message))
    end
end
host.__amalur_half_sync_icon = state
host.set_up_window_components = state.wrapper
print('AMALUR_FINISHER_ICON_INSTALLED|half-width-height')
