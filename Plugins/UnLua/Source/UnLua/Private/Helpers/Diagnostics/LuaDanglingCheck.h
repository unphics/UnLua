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

#pragma once

#include "CoreMinimal.h"
#include "lua.hpp"

/**
 * 防止Lua中的悬空指针(DanglingPointer). 当应该UObject或容器被垃圾回收后, Lua测仍然持有他的指针, 这时就需要检测并清理
 * 工作流:
 *     1. 当Lua调用某个需要检测的函数时, 调用MakeGuard()创建guard
 *     2. 在函数执行过程中, 涉及的struct/container指针会被CaptureStruct/CaptureContainer记录
 *     3. 函数返回时, guard被销毁, 析构函数会:
 *         - 检查StructMap/ContainerMap中是否还有这些指针
 *         - 如果有，清空指针或标记为已释放状态
 *         - 防止后续访问到已经被 UE 垃圾回收的对象
 */
namespace UnLua
{

class FLuaEnv;

class FDanglingCheck {
public:
    static bool Enabled; // 全局开关, 是否启用悬空检查

    // 这是应该RAII的guard, 用于在某个作用域内跟踪捕获的指针
    class FGuard final {
    public:
        explicit FGuard(FDanglingCheck* Owner);
        ~FGuard(); // 析构时检查并清理悬空指针
    private:
        FDanglingCheck* __Owner;
    };

    explicit FDanglingCheck(FLuaEnv* Env);

    TUniquePtr<FGuard> MakeGuard();

    // 捕获struct或container的指针
    void CaptureStruct(lua_State* L, void* Value);
    void CaptureContainer(lua_State* L, void* Value);

private:
    FLuaEnv* __Env;
    int32 __GuardCount; // 当前活跃的guard数量
    TSet<void*> __CapturedStructs; // 捕获的结构体指针集合
    TSet<void*> __CapturedContainers; // 捕获的容器指针集合
};

}
