extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}
 
#include "stdio.h"
 
//new custom hello method
int hello(lua_State * L){
    printf("C++ says: Hello World\n");
 
    return 1;
}
 
int main(int argc, char ** argv){
 
    //iterate all files and execute
    for(int n=1; n<argc; n++){
        const char * file = argv[n];
 
        //create a new lua state
        lua_State * L = luaL_newstate();
 
        //open all libraries
        luaL_openlibs(L);
 
        //register new custom method
        lua_register(L, "hello", hello);
 
        int s = luaL_loadfile(L, file);
 
        if(!s)
            s = lua_pcall(L, 0, LUA_MULTRET, 0);
 
        //show any errors
        if(s){
            printf("Error: %s \n", lua_tostring(L, -1));
            lua_pop(L, 1);
        }
 
        lua_close(L);
    }
 
    return 0;
}
