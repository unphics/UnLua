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

#include "Engine/EngineBaseTypes.h"
#include "Registries/ObjectRegistry.h"
#include "Registries/ClassRegistry.h"
#include "Registries/DelegateRegistry.h"
#include "Registries/FunctionRegistry.h"
#include "Registries/ContainerRegistry.h"
#include "Registries/PropertyRegistry.h"
#include "Registries/EnumRegistry.h"
#include "UnLuaManager.h"
#include "lua.hpp"
#include "ObjectReferencer.h"
#include "HAL/Platform.h"
#include "Diagnostics/LuaDanglingCheck.h"
#include "Diagnostics/LuaDeadLoopCheck.h"
#include "LuaModuleLocator.h"

namespace UnLua
{

class UNLUA_API FLuaEnv : public FUObjectArray::FUObjectDeleteListener {
    friend FClassRegistry;
    friend FDelegateRegistry;
    friend FObjectRegistry;

public:
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnCreated, FLuaEnv&);

    DECLARE_MULTICAST_DELEGATE_OneParam(FOnDestroyed, FLuaEnv&);

    DECLARE_DELEGATE_RetVal_FourParams(bool, FLuaFileLoader, const FLuaEnv& /* Env */, const FString& /* FilePath */, TArray<uint8>&/* Data */, FString&/* RealFilePath */);

    static FOnCreated OnCreated;

    static FOnDestroyed OnDestroyed;

    FLuaEnv();

    virtual ~FLuaEnv() override;

    static TMap<lua_State*, FLuaEnv*>& GetAll();

    static FLuaEnv* FindEnv(const lua_State* L);

    static FLuaEnv& FindEnvChecked(const lua_State* L);

    void Start(const TMap<FString, UObject*>& Args = {});

    void Start(const FString& StartupModuleName, const TMap<FString, UObject*>& Args);

    const FString& GetName();

    void SetName(FString InName);

    virtual void NotifyUObjectDeleted(const UObjectBase* ObjectBase, int32 Index) override;

    virtual void OnUObjectArrayShutdown() override;

    virtual bool TryBind(UObject* Object);

    virtual bool TryReplaceInputs(UObject* Object);

    bool DoString(const FString& Chunk, const FString& ChunkName = "chunk");

    virtual void GC();

    virtual void HotReload();

    FORCEINLINE lua_State* GetMainState() const { return L; }

    void AddThread(lua_State* Thread, int32 ThreadRef);

    int32 FindOrAddThread(lua_State* Thread);

    int32 FindThread(const lua_State* Thread);

    void ResumeThread(int32 ThreadRef);

    UUnLuaManager* GetManager();

    void AddLoader(const FLuaFileLoader Loader);

    void AddBuiltInLoader(const FString InName, lua_CFunction Loader);

    void AddManualObjectReference(UObject* Object);

    void RemoveManualObjectReference(UObject* Object);

protected:
    lua_State* L;

    static int LoadFromBuiltinLibs(lua_State* L);

    static int LoadFromCustomLoader(lua_State* L);

    static int LoadFromFileSystem(lua_State* L);

    static void* DefaultLuaAllocator(void* ud, void* ptr, size_t osize, size_t nsize);

    virtual lua_Alloc GetLuaAllocator() const;

    bool LoadString(lua_State* InL, const TArray<uint8>& Chunk, const FString& ChunkName = "chunk") {
        const char* Bytes = (char*)Chunk.GetData();
        return LoadBuffer(InL, Bytes, Chunk.Num(), TCHAR_TO_UTF8(*ChunkName));
    }

    bool LoadString(lua_State* InL, const FString& Chunk, const FString& ChunkName = "chunk") {
        const FTCHARToUTF8 Bytes(*Chunk);
        return LoadBuffer(InL, Bytes.Get(), Bytes.Length(), TCHAR_TO_UTF8(*ChunkName));
    }

private:
    void _AddSearcher(lua_CFunction Searcher, int Index) const;

    bool LoadBuffer(lua_State* InL, const char* Buffer, const size_t Size, const char* InName);

    void OnAsyncLoadingFlushUpdate();

    void OnWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaTime);

    void _RegisterDelegates();

    void _UnRegisterDelegates();

    static TMap<lua_State*, FLuaEnv*> AllEnvs;
    TMap<FString, lua_CFunction> BuiltinLoaders;
    TArray<FLuaFileLoader> CustomLoaders;
    TArray<FWeakObjectPtr> Candidates; // binding candidates during async loading
    ULuaModuleLocator* ModuleLocator;
    FCriticalSection CandidatesLock;
    FObjectReferencer AutoObjectReference;
    FObjectReferencer ManualObjectReference;
    UUnLuaManager* Manager = nullptr;

    TMap<lua_State*, int32> ThreadToRef;
    TMap<int32, lua_State*> RefToThread;
    FDelegateHandle OnAsyncLoadingFlushUpdateHandle;
    TArray<UInputComponent*> CandidateInputComponents;
    FDelegateHandle OnWorldTickStartHandle;
    FString Name = TEXT("Env_0");
    bool bObjectArrayListenerRegistered;
    bool bStarted;

    // 注册表
public:
    FORCEINLINE FClassRegistry* GetClassRegistry() const { return this->_ClassRegistry; }
    FORCEINLINE FObjectRegistry* GetObjectRegistry() const { return this->_ObjectRegistry; }
    FORCEINLINE FDelegateRegistry* GetDelegateRegistry() const { return this->_DelegateRegistry; }
    FORCEINLINE FFunctionRegistry* GetFunctionRegistry() const { return this->_FunctionRegistry; }
    FORCEINLINE FContainerRegistry* GetContainerRegistry() const { return this->_ContainerRegistry; }
    FORCEINLINE FEnumRegistry* GetEnumRegistry() const { return this->_EnumRegistry; }
    FORCEINLINE FPropertyRegistry* GetPropertyRegistry() const { return this->_PropertyRegistry; }
private:
    /**
     * 类注册表, 管理UClass与Lua类的映射关系
     * 处理类的注册、查找和绑定
     * 提供类的继承关系管理
     * 预注册基础类(如UObject和UClass)以确保核心功能可用
     */
    FClassRegistry* _ClassRegistry;
    /**
     * 对象注册表, 管理UObject与Lua对象的映射关系
     * 跟踪UObject的生命周期, 确保Luau中引用的UObject有效
     * 处理UObject的创建、删除和垃圾回收
     * 提供UObject到Lua表的转换功能
     */
    FObjectRegistry* _ObjectRegistry;
    /**
     * 委托注册表, 管理Lua委托与UDelegate的映射关系
     * 处理委托的创建、绑定和调用
     * 提供委托的生命周期管理
     * 支持单播和多播委托
     */
    FDelegateRegistry* _DelegateRegistry;
    /**
     * 函数注册表, 管理Lua函数与UFunction的映射关系
     * 处理函数的参数转换和返回值处理
     * 提供函数的调用的包装和转发
     * 管理函数的重载和多态
     */
    FFunctionRegistry* _FunctionRegistry;
    /**
     * 容器注册表, 管理Lua容器与UContainer的映射关系
     * 处理容器的创建、删除和垃圾回收
     * 提供容器的生命周期管理
     * 支持容器的遍历和查询
     */
    FContainerRegistry* _ContainerRegistry;
    FPropertyRegistry* _PropertyRegistry;
    FEnumRegistry* _EnumRegistry;

    // 检查器
public:
    FORCEINLINE FDanglingCheck* GetDanglingCheck() const { return this->_DanglingCheck; }
    FORCEINLINE FDeadLoopCheck* GetDeadLoopCheck() const { return this->_DeadLoopCheck; }
private:
    /**
     * 挂起检查器, 检查Lua环境是否存在悬空引用(已删除的UObject仍然被Lua引用)
     * 防止Lua代码访问无效的UObject, 导致内存泄漏
     * 提供悬空引用的清理和警告
     */
    FDanglingCheck* _DanglingCheck;
    /**
     * 死循环检查器, 检查Lua环境是否存在死循环(无限递归调用)
     * 监控Lua代码的执行时间
     * 检测到可能的死循环时终端执行
     */
    FDeadLoopCheck* _DeadLoopCheck;
};

}
