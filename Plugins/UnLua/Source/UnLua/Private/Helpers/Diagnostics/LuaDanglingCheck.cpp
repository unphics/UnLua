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

#include "Helpers/Diagnostics/LuaDanglingCheck.h"
#include "LowLevel.h"
#include "LuaEnv.h"
#include "UnLuaDebugBase.h"

namespace UnLua
{

bool FDanglingCheck::Enabled;

FDanglingCheck::FGuard::FGuard(FDanglingCheck* InOwner): __Owner(InOwner) {
    this->__Owner->__GuardCount++; // 增加计数, 表示进入了一个需要检查的作用域
}

// 当guard被销毁时, 会执行悬空检查
FDanglingCheck::FGuard::~FGuard() {
    this->__Owner->__GuardCount--;

    // 除了StructMap(结构体映射)
    if (this->__Owner->__CapturedStructs.Num() > 0) {
        lua_State* L = this->__Owner->__Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, "StructMap"); // 获取 StructMap 表
        for (void*& StructPtr : this->__Owner->__CapturedStructs) {
            lua_pushlightuserdata(L, StructPtr);
            lua_rawget(L, -2); // 在 StructMap 中查找
            if (lua_isnil(L, -1)) { // 如果是 nil，说明已经被清理
                lua_pop(L, 1);
                continue;
            }

            check(lua_isuserdata(L, -1))
            bool TwoLevelPtr;
            void* Userdata = GetUserdataFast(L, -1, &TwoLevelPtr);
            check(TwoLevelPtr)
            *(void**)Userdata = nullptr; // 将 userdata 指向的指针设为 nullptr（清空悬空指针）

            lua_pop(L, 1);

            // 从 StructMap 中移除
            lua_pushlightuserdata(L, StructPtr);
            lua_pushnil(L);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
        if (this->__Owner->__GuardCount == 0)
            this->__Owner->__CapturedStructs.Empty();
    }

    // 处理ContainerMap(容器映射)
    if (this->__Owner->__CapturedContainers.Num() > 0) {
        lua_State* L = this->__Owner->__Env->GetMainState();
        lua_getfield(L, LUA_REGISTRYINDEX, "ScriptContainerMap");
        for (void*& ContainerPtr : this->__Owner->__CapturedContainers) {
            lua_pushlightuserdata(L, ContainerPtr);
            lua_rawget(L, -2);
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
                continue;
            }

            bool TwoLevelPtr;
            void* Userdata = GetUserdataFast(L, -1, &TwoLevelPtr);
            // 类似逻辑，但设置的是 BIT_RELEASED_TAG 标志
            // 标记为已释放
            SetUserdataFlags(Userdata, 1 << 6); // TODO:BIT_RELEASED_TAG
            check(!TwoLevelPtr)

            lua_pop(L, 1);

            lua_pushlightuserdata(L, ContainerPtr);
            lua_pushnil(L);
            lua_rawset(L, -3);
        }
        lua_pop(L, 1);
        if (this->__Owner->__GuardCount == 0)
            this->__Owner->__CapturedContainers.Empty();
    }
}

FDanglingCheck::FDanglingCheck(FLuaEnv* Env): __Env(Env), __GuardCount(0) {}

TUniquePtr<FDanglingCheck::FGuard> FDanglingCheck::MakeGuard() {
    if (!Enabled)
        return TUniquePtr<FGuard>();
    return MakeUnique<FGuard>(this);
}

void FDanglingCheck::CaptureStruct(lua_State* L, void* Value) {
    if (!this->__GuardCount) // 只有存在活跃 guard 时才捕获
        return;
    this->__CapturedStructs.Add(Value);
}

void FDanglingCheck::CaptureContainer(lua_State* L, void* Value) {
    if (!this->__GuardCount)
        return;
    this->__CapturedContainers.Add(Value);
}

}
