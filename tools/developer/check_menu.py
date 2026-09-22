from pathlib import Path
from lupa.lua51 import LuaRuntime

lua = LuaRuntime()
lua.execute(r'''
files={}; now=100; calls=0; updates=0; visible=false
os.time=function() return now end
io=nil
messages={}; print=function(message) messages[#messages+1]=message end
loadfile=function(path)
 if not files[path] then return nil end
 return function() return files[path] end
end
minimap_win={on_update_event=function() updates=updates+1 end}
pause_screen={on_update_event=function() updates=updates+1 end}
ledger_win={m_window=42}
WINDOW={is_visible=function(w) assert(w==42);return visible end}
UI_State_MGR={show_window=function(w,flag) assert(w==42 and flag==false);calls=calls+1;visible=true end}
function ticks() for i=1,10 do minimap_win.on_update_event() end end
function request(nonce,session,expiration)
 files['.\\mods\\amalur_menu_request.lua']={session=session or amalur_menu_state.session,nonce=nonce,expires=expiration or now+4}
 ticks()
 return messages[#messages] or ''
end
''')
script = Path('tools/developer/amalur_menu.lua').read_text()
lua.execute(script)
lua.execute('''
ticks();assert(calls==0 and updates==10)
request('1_1','old_session');assert(calls==0)
assert(request('1_2',nil,90):find('ERROR'));assert(calls==0)
assert(request('1_3'):find('OK'));assert(calls==1)
request('1_3');assert(calls==1)
assert(request('1_4'):find('already open'));assert(calls==1)
visible=false;ledger_win.m_window=nil
assert(request('1_5'):find('not initialized'));assert(calls==1)
ledger_win.m_window=42
UI_State_MGR.show_window=function() calls=calls+1;error('native failure') end
assert(request('1_6'):find('ERROR'));assert(calls==2)
request('1_6');assert(calls==2)
''')
lua.execute(script)  # Reload does not wrap existing wrappers or replay requests.
lua.execute('''local before=updates;ticks();assert(updates==before+10 and calls==2)
-- The pause callback can service requests when minimap updates stop.
UI_State_MGR.show_window=function() calls=calls+1;visible=true end
''')
# Use the same request helper but advance only the pause host this time.
lua.execute('''ticks=function() for i=1,10 do pause_screen.on_update_event() end end
assert(request('1_7'):find('OK'));assert(calls==3)''')
print('PASS: no startup action, session/expiry guards, duplicate requests, native menu open, already-open behavior, missing menu, engine failure, reload and paused UI host')

lua.execute("""UI_State_MGR.update_state_manager=function(a,b) return a,nil,b end""")
lua.execute(script)
lua.execute("""visible=false
ticks=function() for i=1,10 do local a,b,c=UI_State_MGR.update_state_manager(3,5);assert(a==3 and b==nil and c==5) end end
assert(request('1_8'):find('OK'));assert(calls==4)
local saved=UI_State_MGR.update_state_manager
""")
print('PASS: UI state update services pause when window callbacks stop and preserves native return values.')
