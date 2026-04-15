// Tencent is pleased to support the open source community by making UnLua available.
// 
// Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the MIT License (the "License"); 
// you may not use this file except in compliance with the License. You may obtain a copy of the License at
//
// http://opensource.org/licenses/MIT
//
// Unless required by applicable law or agreed to in writing, 
// software distributed under the License is distributed on an "AS IS" BASIS, 
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
// See the License for the specific language governing permissions and limitations under the License.

#include "ObjectRegistry.h"
#include "LowLevel.h"
#include "LuaEnv.h"
#include "UnLuaDelegates.h"

namespace UnLua
{

/**
 * 为什么要创建弱引用表: 普通表会阻止对象倍GC, 弱引用表的value是弱引用, 当对象没用其他引用时可以被GC回收
 */
static const char* REGISTRY_KEY = "UnLua_ObjectMap"; // 对象映射表键, 存储所有已绑定的UObject对象
static const char* MANUAL_REF_PROXY_MAP = "UnLua_ManualRefProxyMap"; // 手动引用代理映射, 存储手动引用的代理对象

static int ReleaseSharedPtr(lua_State* L) {
    // 栈顶是要被gc的userdata, 当luaGC回收userdata时会吧要回收的userdata压入栈顶然后调用__gc, 此时栈上只有一个元素就是那个userdata
    TSharedPtr<void>* ptr = (TSharedPtr<void>*)lua_touserdata(L, 1);
    ptr->Reset();
    return 0;
}

static int ReleaseManualRef(lua_State* L) {
    FLuaEnv& env = FLuaEnv::FindEnvChecked(L);
    FManualRefProxy* proxy = (FManualRefProxy*)lua_touserdata(L, 1);
    UObject* Object = proxy->Object.Get();
    if (!Object) {
        return 0;
    }
    // 获取注册表中的弱引用表, 压入栈顶; [Proxy, Map]
    lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
    // 把Object指针压栈; [Proxy, Map, ObjectPtr]
    lua_pushlightuserdata(L, Object);
    // 用-2位置的Map查找-1位置的ObjectPtr作为Key, 获取Map[ObjectPtr]; [Proxy, Map, result]
    if (lua_rawget(L, -2) == LUA_TNIL) {
        env.RemoveManualObjectReference(Object);
    }
    return 0;
}

FObjectRegistry::FObjectRegistry(FLuaEnv* InEnv): Env(InEnv) {
    lua_State* L = InEnv->GetMainState(); // 主状态机

    // 创建弱引用表
    lua_pushstring(L, REGISTRY_KEY); // 入栈 "UnLua_ObjectMap"
    LowLevel::CreateWeakValueTable(L); // 创建弱引用表
    lua_rawset(L, LUA_REGISTRYINDEX); // LUA_REGISTRYINDEX
    // ["UnLua_ObjectMap"] = 弱引用表

    lua_pushstring(L, MANUAL_REF_PROXY_MAP); // 入栈: "UnLua_ManualRefProxyMap"
    LowLevel::CreateWeakValueTable(L); // 创建弱引用表
    lua_rawset(L, LUA_REGISTRYINDEX); // LUA_REGISTRYINDEX
    // ["UnLua_ManualRefProxyMap"] = 弱引用表
     
    // 创建TSharedPtr元表; 作用是当Lua中的TSharedPtr的userdata被GC时, 调用ReleaseSharedPtr
    luaL_newmetatable(L, "TSharedPtr"); // 创建新元表并压栈
    lua_pushstring(L, "__gc"); // 入栈: "__gc"
    lua_pushcfunction(L, ReleaseSharedPtr); // 入栈: GC函数
    lua_rawset(L, -3); // 元表["__gc"] = ReleaseSharedPtr

    // 创建UnLuaManualRefProxy元表; 作用是当手动引用的代理对象被GC时, 调用ReleaseManualRef清理手动引用
    luaL_newmetatable(L, "UnLuaManualRefProxy");
    lua_pushstring(L, "__gc");
    lua_pushcfunction(L, ReleaseManualRef);
    lua_rawset(L, -3);

    lua_pop(L, 1); // 栈清理; 弹出luaL_newmetatable留在栈上的元表, 保持栈平衡
}

void FObjectRegistry::NotifyUObjectDeleted(UObject* Object) {
    this->Unbind(Object);
}

void FObjectRegistry::NotifyUObjectLuaGC(UObject* Object) {
    this->Env->AutoObjectReference.Remove(Object);
}

// 将UObject转换为Lua克访问的对象, 并缓存起来避免重复创建
void FObjectRegistry::Push(lua_State* L, UObject* Object) {
    if (!Object) {
        lua_pushnil(L);
        return;
    }
    // 获取REGISTRY_KEY表, 压入栈顶; [ObjectMap]
    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
    // 把Object指针压栈, [ObjectMap, ObjectPtr]
    lua_pushlightuserdata(L, Object);
    // 用ObjectPtr在ObjectMap中查找; [ObjectMap, ObjectMap[ObjectPtr]]
    // 返回值赋给Type; [ObjectMap, result]
    const int Type = lua_rawget(L, -2);
    if (Type == LUA_TNIL) { // 如果没找到则创建新对象; 如果ObjectMap[ObjectPtr]不存在
        lua_pop(L, 1); // 弹出 nil
        PushObjectCore(L, Object); // 创建Lua对象, 压入栈顶; [ObjectMap, RAW_UOBJECT]

        lua_pushlightuserdata(L, Object);
        lua_pushvalue(L, -2); // 复制 RAW_UOBJECT
        lua_rawset(L, -4); // ObjectMap[ObjectPtr] = RAW_UOBJECT; [ObjectMap, RAW_UOBJECT]
        ObjectRefs.Add(Object, LUA_NOREF); // 记录到 C++ 侧映射表
    }
    lua_remove(L, -2);
}

int FObjectRegistry::Bind(UObject* Object)
{
    if (const auto Exists = ObjectRefs.Find(Object))
    {
        if (*Exists != LUA_NOREF)
            return *Exists;
    }

    const auto L = Env->GetMainState();

    int OldTop = lua_gettop(L);

    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
    lua_pushlightuserdata(L, Object);
    lua_newtable(L); // create a Lua table ('INSTANCE')
    PushObjectCore(L, Object); // push UObject ('RAW_UOBJECT')
    lua_pushstring(L, "Object");
    lua_pushvalue(L, -2);
    lua_rawset(L, -4); // INSTANCE.Object = RAW_UOBJECT

    // in some case may occur module or object metatable can 
    // not be found problem
    const auto Class = Object->IsA<UClass>() ? static_cast<UClass*>(Object) : Object->GetClass();
    const auto ClassBoundRef = Env->GetManager()->GetBoundRef(Class);
    int32 TypeModule = lua_rawgeti(L, LUA_REGISTRYINDEX, ClassBoundRef); // push the required module/table ('REQUIRED_MODULE') to the top of the stack
    int32 TypeMetatable = lua_getmetatable(L, -2); // get the metatable ('METATABLE_UOBJECT') of 'RAW_UOBJECT' 
    if (TypeModule != LUA_TTABLE || TypeMetatable == LUA_TNIL)
    {
        lua_pop(L, lua_gettop(L) - OldTop);
        return LUA_REFNIL;
    }

#if ENABLE_CALL_OVERRIDDEN_FUNCTION
    lua_pushstring(L, "Overridden");
    lua_pushvalue(L, -2);
    lua_rawset(L, -4);
#endif
    lua_setmetatable(L, -2); // REQUIRED_MODULE.metatable = METATABLE_UOBJECT
    lua_setmetatable(L, -3); // INSTANCE.metatable = REQUIRED_MODULE
    lua_pop(L, 1);

    lua_pushvalue(L, -1);
    const auto Ret = luaL_ref(L, LUA_REGISTRYINDEX);
    ObjectRefs.Add(Object, Ret);

    FUnLuaDelegates::OnObjectBinded.Broadcast(Object); // 'INSTANCE' is on the top of stack now

    lua_rawset(L, -3);
    lua_pop(L, 1);
    return Ret;
}

bool FObjectRegistry::IsBound(const UObject* Object) const {
    const int32* exists = this->ObjectRefs.Find(Object);
    return exists && *exists != LUA_NOREF;
}

int FObjectRegistry::GetBoundRef(const UObject* Object) const {
    const int32* refIndex = this->ObjectRefs.Find(Object);
    if (refIndex) {
        return *refIndex;
    }
    return LUA_NOREF;
}

void FObjectRegistry::Unbind(UObject* Object)
{
    int32 Ref;
    if (!ObjectRefs.RemoveAndCopyValue(Object, Ref))
        return;

    const auto L = Env->GetMainState();
    const auto Top = lua_gettop(L);
    RemoveFromObjectMapAndPushToStack(Object);

    if (Ref == LUA_NOREF)
    {
        if (lua_isnil(L, -1))
        {
            lua_pop(L, 1);
            return;
        }
        check(lua_isuserdata(L, -1));
        bool bTwoLvlPtr;
        void* Userdata = GetUserdataFast(L, -1, &bTwoLvlPtr);
        check(bTwoLvlPtr)
        *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;
        lua_settop(L, Top);
        return;
    }

    check(lua_istable(L, -1));
    luaL_unref(L, LUA_REGISTRYINDEX, Ref);
    FUnLuaDelegates::OnObjectUnbinded.Broadcast(Object); // object instance ('INSTANCE') is on the top of stack now

    lua_pushstring(L, "Object");
    lua_rawget(L, -2);
    void* Userdata = lua_touserdata(L, -1);
    *((void**)Userdata) = (void*)LowLevel::ReleasedPtr;

    lua_settop(L, Top);
}

void FObjectRegistry::AddManualRef(lua_State* L, UObject* Object)
{
    lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
    lua_pushlightuserdata(L, Object);
    if (lua_rawget(L, -2) == LUA_TNIL)
    {
        lua_pop(L, 1);
        Env->AddManualObjectReference(Object);
        auto Ptr = lua_newuserdata(L, sizeof(FManualRefProxy));
        auto Proxy = new(Ptr)FManualRefProxy;
        Proxy->Object = Object;
        luaL_getmetatable(L, "UnLuaManualRefProxy");
        lua_setmetatable(L, -2);
        lua_pushlightuserdata(L, Object);
        lua_pushvalue(L, -2);
        lua_rawset(L, -4);
    }
    lua_remove(L, -2);
}

void FObjectRegistry::RemoveManualRef(UObject* Object)
{
    const auto L = Env->GetMainState();
    lua_getfield(L, LUA_REGISTRYINDEX, MANUAL_REF_PROXY_MAP);
    lua_pushlightuserdata(L, Object);
    lua_pushnil(L);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    Env->RemoveManualObjectReference(Object);
}

void FObjectRegistry::RemoveFromObjectMapAndPushToStack(UObject* Object)
{
    const auto L = Env->GetMainState();
    lua_getfield(L, LUA_REGISTRYINDEX, REGISTRY_KEY);
    lua_pushlightuserdata(L, Object);
    lua_rawget(L, -2);
    lua_pushlightuserdata(L, Object);
    lua_pushnil(L);
    lua_rawset(L, -4);
    lua_remove(L, -2);
}

}
