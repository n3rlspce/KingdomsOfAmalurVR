from pathlib import Path
from lupa.lua51 import LuaRuntime
lua=LuaRuntime()
lua.execute('''amalur_dispatch_state={executing=true}; might=10; hp=0; adds=0
get_player=function() return 42 end
PLAYER={get_unmodified_attribute=function(name) assert(name=='Might'); return might end, increment_attribute=function(name,n) assert(name=='Might'); might=might+n; adds=adds+1 end}
ACTOR={get_max_health=function(p) assert(p==42); return 500+2.5*might end,get_current_health=function() return hp end,modify_health=function(p,n) assert(p==42);hp=hp+n end}''')
lua.execute(Path('tools/developer/amalur_dev.lua').read_text())
lua.execute('''assert(hp==0 and adds==0);amalur_dispatch_state.executing=false;assert(not pcall(amalur_dev.boost_health,10000) and adds==0)
amalur_dispatch_state.executing=true;amalur_dev.boost_health(10000);assert(hp==10000 and adds==2);amalur_dev.boost_health(10000);assert(adds==2)
ACTOR.get_max_health=function() return 500 end;assert(not pcall(amalur_dev.boost_health,10000) and adds==3)''')
print('PASS: measured HP scaling, refill, dispatch guard, idempotence, stop on absent scaling')
