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

#if WITH_EDITOR
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Modules/ModuleManager.h"
#endif

#if ALLOW_CONSOLE
#include "Engine/Console.h"
#include "UnLuaConsoleCommands.h"
#endif

#include "Engine/World.h"
#include "UnLuaModule.h"
#include "DefaultParamCollection.h"
#include "GameDelegates.h"
#include "LuaEnvLocator.h"
#include "UnLuaDebugBase.h"
#include "UnLuaInterface.h"
#include "UnLuaSettings.h"
#include "GameFramework/PlayerController.h"
#include "Registries/ClassRegistry.h"
#include "Registries/EnumRegistry.h"

#define LOCTEXT_NAMESPACE "FUnLuaModule"

namespace UnLua
{

class FUnLuaModule : public IUnLuaModule, public FUObjectArray::FUObjectCreateListener, public FUObjectArray::FUObjectDeleteListener {
public:
    virtual void StartupModule() override {
#if WITH_EDITOR // 编辑器环境下加载UnLuaEditor模块
        FModuleManager::Get().LoadModule(TEXT("UnLuaEditor"));
#endif
        this->RegisterSettings(); // 注册UnLua的配置项添加到编辑器的项目设置中
#if ALLOW_CONSOLE // 允许在控制台中使用UnLua的命令
        this->ConsoleCommands = MakeUnique<FUnLuaConsoleCommands>(this);
#endif
        // 注册PostLoadMapWithWorld委托，用于在加载地图后初始化UnLua的环境
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddRaw(this, &FUnLuaModule::PostLoadMapWithWorld);

        ::CreateDefaultParamCollection(); // 初始化UnLua的默认参数系统, 用于处理函数参数默认值

#if AUTO_UNLUA_STARTUP // 自动启动UnLua模块
#if WITH_EDITOR
        /**
         * 如果不在游戏运行中(即在编辑器环境下)绑定PIE相关的委托:
         * 1. PreBeginPIE: 在开始播放PIE前调用, 用于初始化UnLua的环境
         * 2. PostPIEStarted: 在PIE启动后调用, 用于注册UnLua的命令
         * 3. EndPIE: 在PIE结束时调用, 用于清理UnLua的环境
         */
        if (!IsRunningGame()) {
            FEditorDelegates::PreBeginPIE.AddRaw(this, &FUnLuaModule::OnPreBeginPIE);
            FEditorDelegates::PostPIEStarted.AddRaw(this, &FUnLuaModule::OnPostPIEStarted);
            FEditorDelegates::EndPIE.AddRaw(this, &FUnLuaModule::OnEndPIE);
            FGameDelegates::Get().GetEndPlayMapDelegate().AddRaw(this, &FUnLuaModule::OnEndPlayMap);
        }

        // 如果在游戏运行中或DS中直接激活UnLua
        if (IsRunningGame() || IsRunningDedicatedServer())
#endif
            this->SetActive(true);
#endif
    }

    virtual void ShutdownModule() override {
        this->UnregisterSettings();
        this->SetActive(false);
    }

    virtual bool IsActive() override {
        return this->bIsActive;
    }

    virtual void SetActive(const bool InActive) override {
        if (this->bIsActive == InActive)
            return;

        if (InActive) {
            // 系统错误处理绑定; 当系统发生错误或断言时调用OnSystemError方法, 打印Lua调用栈信息, 帮助调试
            this->OnHandleSystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddRaw(this, &FUnLuaModule::OnSystemError);
            this->OnHandleSystemEnsureHandle = FCoreDelegates::OnHandleSystemEnsure.AddRaw(this, &FUnLuaModule::OnSystemError);

            // 对象生命周期监听; 当新对象创建或删除时调用NotifyUObjectCreated(尝试为该对象绑定Lua)或NotifyUObjectDeleted(清理相关注册表信息)方法
            ::GUObjectArray.AddUObjectCreateListener(this);
            ::GUObjectArray.AddUObjectDeleteListener(this);

            // 加载Lua设置并应用
            const UUnLuaSettings& Settings = *GetMutableDefault<UUnLuaSettings>(); // 获取设置对象
            // 创建Lua环境定位器, 用于管理不同对象的Lua环境
            const auto EnvLocatorClass = *Settings.EnvLocatorClass == nullptr ? ULuaEnvLocator::StaticClass() : *Settings.EnvLocatorClass;
            this->EnvLocator = NewObject<ULuaEnvLocator>(GetTransientPackage(), EnvLocatorClass);
            this->EnvLocator->AddToRoot();
            FDeadLoopCheck::Timeout = Settings.DeadLoopCheck; // 配置死循环检查超时时间
            FDanglingCheck::Enabled = Settings.DanglingCheck; // 配置悬垂指针检查是否启用

            // 预绑定设置中指定的类; 遍历所有UE类, 检查是否是设置中指定的预绑定类的子类, 如果是则尝试绑定Lua环境到该类的实例
            for (const auto ueUClass : TObjectRange<UClass>()) { // 遍历所有UE类
                for (const FSoftClassPath& prebindClassPath : Settings.PreBindClasses) { // 遍历用户配置的预绑定类路径
                    if (!prebindClassPath.IsValid())
                        continue; // 跳过无效的路径

                    const UClass* targetClass = prebindClassPath.ResolveClass(); // 解析为实际的类对象
                    if (!targetClass)
                        continue; // 跳过无效的类

                    if (ueUClass->IsChildOf(targetClass)) {
                        UnLua::FLuaEnv* env = this->EnvLocator->Locate(ueUClass); // 通过环境定位器找到适合该类的Lua环境
                        env->TryBind(ueUClass); // 尝试绑定Lua环境到该类的实例
                        break;
                    }
                }
            }
        } else {
            // 移除系统错误委托处理绑定
            FCoreDelegates::OnHandleSystemError.Remove(this->OnHandleSystemErrorHandle);
            FCoreDelegates::OnHandleSystemEnsure.Remove(this->OnHandleSystemEnsureHandle);
            // 移除对象生命周期监听
            ::GUObjectArray.RemoveUObjectCreateListener(this);
            ::GUObjectArray.RemoveUObjectDeleteListener(this);
            // 重置和清理Lua环境定位器, 释放所有绑定的Lua环境
            this->EnvLocator->Reset();
            this->EnvLocator->RemoveFromRoot();
            this->EnvLocator = nullptr;
            // 清理注册表
            FClassRegistry::Cleanup();
            FEnumRegistry::Cleanup();

            // 恢复函数覆盖
            for (UClass* Class : TObjectRange<UClass>()) {
                if (Class->ImplementsInterface(UUnLuaInterface::StaticClass())) // 检查是否实现了UUnLuaInterface接口
                    ULuaFunction::RestoreOverrides(Class);
            }
        }

        this->bIsActive = InActive;
    }

    virtual FLuaEnv* GetEnv(UObject* Object) override {
        if (!this->bIsActive)
            return nullptr;
        return this->EnvLocator->Locate(Object);
    }

    virtual void HotReload() override {
        if (!this->bIsActive)
            return;
        this->EnvLocator->HotReload();
    }

private:
    virtual void NotifyUObjectCreated(const UObjectBase* InObjectBase, int32 InIndex) override {
        // UE_LOG(LogTemp, Log, TEXT("NotifyUObjectCreated : %p"), InObjectBase);
        if (!this->bIsActive)
            return;

        UObject* object = const_cast<UObject*>(static_cast<const UObject*>(InObjectBase));

        const auto env = this->EnvLocator->Locate(object);
        // UE_LOG(LogTemp, Log, TEXT("Locate %s for %s"), *env->GetName(), *InObjectBase->GetFName().ToString());
        env->TryBind(object);
        env->TryReplaceInputs(object); // 尝试替换对象的输入处理
    }

    virtual void NotifyUObjectDeleted(const UObjectBase* InObjectBase, int32 InIndex) override {
        // UE_LOG(LogTemp, Log, TEXT("NotifyUObjectDeleted : %p"), InObjectBase);
        if (!this->bIsActive)
            return;

        if (FClassRegistry::StaticUnregister(InObjectBase)) // 注销类注册表中的对象
            return;

        FEnumRegistry::StaticUnregister(InObjectBase); // 注销枚举注册表中的对象
    }

    // UnLua在UE对象系统(GUObjectArray, 全局UObject数组)关闭时的清理回调
    virtual void OnUObjectArrayShutdown() override {
        if (!this->bIsActive)
            return;

        // 移除对象生命周期监听
        ::GUObjectArray.RemoveUObjectCreateListener(this);
        ::GUObjectArray.RemoveUObjectDeleteListener(this);

        this->bIsActive = false;
    }

    void OnSystemError() const {
        if (!this->bPrintLuaStackOnSystemError)
            return;

        if (!::IsInGameThread())
            return;

        for (TPair<lua_State*, FLuaEnv*>& Pair : FLuaEnv::GetAll()) {
            if (!Pair.Key || !Pair.Value)
                continue;

            UE_LOG(LogUnLua, Log, TEXT("%s:"), *Pair.Value->GetName())
            UnLua::PrintCallStack(Pair.Key);
            UE_LOG(LogUnLua, Log, TEXT(""))
        }

        if (GLog)
            GLog->Flush();
    }

#if WITH_EDITOR

    void OnPreBeginPIE(bool bIsSimulating) {
        this->SetActive(true);
    }

    void OnPostPIEStarted(bool bIsSimulating) {
        UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);
        if (EditorEngine)
            this->PostLoadMapWithWorld(EditorEngine->PlayWorld);
    }

    void OnEndPIE(bool bIsSimulating) {
        // SetActive(false);
    }

    void OnEndPlayMap() {
        this->SetActive(false);
    }

#endif

    void RegisterSettings() {
#if WITH_EDITOR
        ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
        if (!SettingsModule)
            return;

        const auto Section = SettingsModule->RegisterSettings("Project", "Plugins", "UnLua", LOCTEXT("UnLuaEditorSettingsName", "UnLua"), LOCTEXT("UnLuaEditorSettingsDescription", "UnLua Runtime Settings"), GetMutableDefault<UUnLuaSettings>());
        Section->OnModified().BindRaw(this, &FUnLuaModule::OnSettingsModified);
#endif

#if ENGINE_MAJOR_VERSION >=5 && !WITH_EDITOR
        // UE5下打包后没有从{PROJECT}/Config/DefaultUnLua.ini加载，这里强制刷新一下
        FString UnLuaIni = TEXT("UnLua");
        GConfig->LoadGlobalIniFile(UnLuaIni, *UnLuaIni, nullptr, true);
        UUnLuaSettings::StaticClass()->GetDefaultObject()->ReloadConfig();
#endif

        auto& Settings = *GetDefault<UUnLuaSettings>();
        bPrintLuaStackOnSystemError = Settings.bPrintLuaStackOnSystemError;
    }

    void UnregisterSettings() {
#if WITH_EDITOR
        ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
        if (SettingsModule)
            SettingsModule->UnregisterSettings("Project", "Plugins", "UnLua");
#endif
    }

    bool OnSettingsModified() {
        auto& Settings = *GetDefault<UUnLuaSettings>();
        bPrintLuaStackOnSystemError = Settings.bPrintLuaStackOnSystemError;
        return true;
    }

    void PostLoadMapWithWorld(UWorld* InWorld) const {
        if (!InWorld || !this->bIsActive)
            return;

        UnLua::FLuaEnv* env = this->EnvLocator->Locate(InWorld);
        if (!env)
            return;

        UUnLuaManager* Manager = env->GetManager();
        if (!Manager)
            return;

        Manager->OnMapLoaded(InWorld);
    }

    bool bIsActive = false;
    bool bPrintLuaStackOnSystemError = false;
    ULuaEnvLocator* EnvLocator = nullptr;
    FDelegateHandle OnHandleSystemErrorHandle;
    FDelegateHandle OnHandleSystemEnsureHandle;
#if ALLOW_CONSOLE
    TUniquePtr<FUnLuaConsoleCommands> ConsoleCommands;
#endif
};

}

IMPLEMENT_MODULE(UnLua::FUnLuaModule, UnLua)

#undef LOCTEXT_NAMESPACE
