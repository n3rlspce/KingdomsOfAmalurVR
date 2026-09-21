-- Recovery UI requests run on the game thread, independently of player state.
-- Never execute engine calls on the framework's console thread.
local state = _G.amalur_menu_state
local base = '.\\mods\\amalur_menu_'
local function write(name, value)
    local file = io.open(base .. name .. '.txt', 'w')
    if file then file:write(value); file:close() end
end
if not state then
    state = {frames = 0, seen = {}, hooks = {}}
    state.session = tostring(os.time()) .. '_' .. string.gsub(tostring(state), '[^%w]', '')
    _G.amalur_menu_state = state
end
local function open_menu()
    assert(type(ledger_win) == 'table' and ledger_win.m_window ~= nil,
           'Pause menu is not initialized; return to the title screen after restarting.')
    assert(UI_State_MGR and type(UI_State_MGR.show_window) == 'function', 'Menu API unavailable.')
    assert(WINDOW and type(WINDOW.is_visible) == 'function', 'Window API unavailable.')
    local window = ledger_win.m_window
    if WINDOW.is_visible(window) then return 'Pause menu already open.' end
    -- This is the game's ledger_win.on_input_entering open branch. The native
    -- SHOW event handles pause counts, LedgerMode input and inventory portrait.
    UI_State_MGR.show_window(window, false)
    assert(WINDOW.is_visible(window), 'The game rejected opening the pause menu.')
    return 'Pause menu opened.'
end
local function poll()
    state.frames = state.frames + 1
    if state.frames % 10 ~= 0 then return end
    write('ready', state.session .. '\n' .. tostring(os.time()))
    local file = io.open(base .. 'request.txt', 'r')
    if not file then return end
    local session, nonce, expires = file:read('*l'), file:read('*l'), file:read('*l')
    file:close()
    if session ~= state.session or type(nonce) ~= 'string' or #nonce > 64 or
       not string.match(nonce, '^%d+_%d+$') or state.seen[nonce] then return end
    state.seen[nonce] = true -- consume before engine calls; never retry
    expires = tonumber(expires)
    local ok, result
    if not expires or expires < os.time() or expires > os.time() + 10 then
        ok, result = false, 'Pause request expired; nothing changed.'
    else
        ok, result = pcall(open_menu)
    end
    result = string.gsub(tostring(result), '[\r\n]', ' ')
    write('result', nonce .. '\n' .. (ok and 'OK' or 'ERROR') .. '\n' .. result)
    print('AMALUR_MENU|' .. nonce .. '|' .. (ok and 'OK|' or 'ERROR|') .. result)
end
for _, name in ipairs({'minimap_win', 'pause_screen', 'ledger_win'}) do
    local host = _G[name]
    if type(host) == 'table' and type(host.on_update_event) == 'function' and
       host.on_update_event ~= state.hooks[name] then
        local original = host.on_update_event
        local wrapper = function(...)
            original(...)
            poll()
        end
        state.hooks[name] = wrapper
        host.on_update_event = wrapper
        print('AMALUR_MENU_READY|' .. name)
    end
end
