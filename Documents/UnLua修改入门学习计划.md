# UnLua修改入门学习计划

本计划以"改得动unlua"为第一目标，通过系统学习UnLua的核心组件和关键文件，掌握修改和扩展UnLua的能力。

## 一、核心文件结构与初始化流程

### 1.1 核心入口文件
- [ ] **UnLua.cpp/.h**：UnLua的主入口文件
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/UnLua.cpp`
  - [ ] 功能：插件初始化、模块注册、核心流程控制
  - [ ] 关键函数：`FUnLuaModule::StartupModule()`、`FUnLuaModule::ShutdownModule()`

### 1.2 Lua环境初始化
- [ ] **LuaEnv.cpp/.h**：Lua环境管理
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaEnv.cpp`
  - [ ] 功能：创建和管理Lua虚拟机实例
  - [ ] 关键函数：`FLuaEnv::FLuaEnv()`、`FLuaEnv::Initialize()`、`FLuaEnv::ExecuteFile()`

### 1.3 模块注册机制
- [ ] **ModuleManager.cpp/.h**：模块管理
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/ModuleManager.cpp`
  - [ ] 功能：管理UnLua的各个功能模块
  - [ ] 关键函数：`FModuleManager::Initialize()`、模块注册流程

## 二、反射系统与UE交互

### 2.1 反射系统核心
- [ ] **LuaReflection.cpp/.h**：UE反射系统与Lua的桥接
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaReflection.cpp`
  - [ ] 功能：将UE的反射信息转换为Lua可访问的形式
  - [ ] 关键函数：`FLuaReflection::RegisterClass()`、`FLuaReflection::GetProperty()`

### 2.2 UE对象在Lua中的表示
- [ ] **LuaObject.cpp/.h**：UE对象包装器
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaObject.cpp`
  - [ ] 功能：将UE对象包装为Lua表，处理对象的生命周期
  - [ ] 关键函数：`FLuaObject::Push()`、`FLuaObject::GetUObject()`

### 2.3 蓝图与Lua交互
- [ ] **LuaBlueprint.cpp/.h**：蓝图与Lua的交互
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaBlueprint.cpp`
  - [ ] 功能：处理蓝图类的Lua绑定
  - [ ] 关键函数：`FLuaBlueprint::RegisterBlueprintClass()`、`FLuaBlueprint::CreateInstance()`

## 三、注册表系统

### 3.1 全局注册表
- [ ] **LuaRegistry.cpp/.h**：全局注册表
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaRegistry.cpp`
  - [ ] 功能：管理UE类与Lua表的映射关系
  - [ ] 关键函数：`FLuaRegistry::RegisterClass()`、`FLuaRegistry::GetClass()`

### 3.2 类注册机制
- [ ] **ClassRegister.cpp/.h**：类注册
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/ClassRegister.cpp`
  - [ ] 功能：处理UE类的注册过程
  - [ ] 关键函数：`FClassRegister::Register()`、`FClassRegister::GetRegisteredClass()`

## 四、类型绑定系统

### 4.1 基础类型绑定
- [ ] **LuaBase.cpp/.h**：基础类型绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaBase.cpp`
  - [ ] 功能：绑定UE的基础类型（FString、FName等）
  - [ ] 关键函数：`FLuaBase::Register()`、类型转换函数

### 4.2 数学类型绑定
- [ ] **LuaVector.cpp/.h**、**LuaRotator.cpp/.h**等：数学类型绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaVector.cpp`等
  - [ ] 功能：绑定UE的数学类型
  - [ ] 关键函数：数学类型的注册和转换

### 4.3 容器类型绑定
- [ ] **LuaArray.cpp/.h**、**LuaMap.cpp/.h**等：容器类型绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaArray.cpp`等
  - [ ] 功能：绑定UE的容器类型
  - [ ] 关键函数：容器类型的注册和操作

## 五、函数与委托绑定

### 5.1 函数绑定
- [ ] **LuaFunction.cpp/.h**：函数绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaFunction.cpp`
  - [ ] 功能：处理UE函数的Lua绑定
  - [ ] 关键函数：`FLuaFunction::Create()`、`FLuaFunction::Execute()`

### 5.2 委托绑定
- [ ] **LuaDelegate.cpp/.h**：委托绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaDelegate.cpp`
  - [ ] 功能：处理UE委托的Lua绑定
  - [ ] 关键函数：`FLuaDelegate::Bind()`、`FLuaDelegate::Execute()`

## 六、堆栈操作与数据转换

### 6.1 堆栈操作
- [ ] **LuaStack.cpp/.h**：堆栈操作
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaStack.cpp`
  - [ ] 功能：处理C++与Lua之间的数据交换
  - [ ] 关键函数：`FLuaStack::Push()`、`FLuaStack::Pop()`、类型转换函数

### 6.2 数据转换
- [ ] **LuaConversion.cpp/.h**：数据转换
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaConversion.cpp`
  - [ ] 功能：处理不同类型之间的转换
  - [ ] 关键函数：各种类型转换函数

## 七、错误处理与调试

### 7.1 错误处理
- [ ] **LuaErrorHandling.cpp/.h**：错误处理
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaErrorHandling.cpp`
  - [ ] 功能：处理Lua执行过程中的错误
  - [ ] 关键函数：错误捕获和处理函数

### 7.2 调试支持
- [ ] **LuaDebug.cpp/.h**：调试支持
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/LuaDebug.cpp`
  - [ ] 功能：提供调试相关的功能
  - [ ] 关键函数：调试信息收集和处理

## 八、配置系统

### 8.1 配置文件
- [ ] **UnLuaSettings.h**：配置设置
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Public/UnLuaSettings.h`
  - [ ] 功能：定义UnLua的配置选项
  - [ ] 关键配置项：Lua文件路径、默认模块等

### 8.2 配置加载
- [ ] **UnLuaSettings.cpp**：配置加载
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/UnLuaSettings.cpp`
  - [ ] 功能：加载和应用配置
  - [ ] 关键函数：配置加载和验证

## 九、扩展与自定义

### 9.1 公共接口
- [ ] **UnLuaInterface.h**：公共接口
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Public/UnLuaInterface.h`
  - [ ] 功能：定义UnLua的公共接口
  - [ ] 关键接口：`IUnLuaInterface`及其方法

### 9.2 自定义绑定
- [ ] **CustomBinding.cpp/.h**：自定义绑定
  - [ ] 位置：`Plugins/UnLua/Source/UnLua/Private/CustomBinding.cpp`
  - [ ] 功能：处理自定义类型的绑定
  - [ ] 关键函数：自定义类型注册函数

## 十、实战修改练习

### 10.1 简单修改
- [ ] **修改Lua文件加载路径**
  - [ ] 找到相关代码：`LuaEnv.cpp`中的文件加载逻辑
  - [ ] 理解当前实现：如何确定Lua文件的搜索路径
  - [ ] 进行修改：添加自定义搜索路径
  - [ ] 测试验证：确保修改后的路径生效

### 10.2 扩展类型绑定
- [ ] **添加新类型绑定**
  - [ ] 找到类型绑定的注册位置：`ModuleManager.cpp`或相关文件
  - [ ] 理解现有类型的绑定方式：参考`LuaBase.cpp`等
  - [ ] 实现新类型的绑定：添加类型转换函数
  - [ ] 测试验证：确保新类型在Lua中可访问

### 10.3 修改反射行为
- [ ] **调整反射系统行为**
  - [ ] 找到反射系统的核心代码：`LuaReflection.cpp`
  - [ ] 理解当前反射行为：如何处理UE类的属性和方法
  - [ ] 进行修改：调整反射行为以满足需求
  - [ ] 测试验证：确保修改后的反射行为正确

### 10.4 性能优化
- [ ] **优化C++与Lua通信**
  - [ ] 找到性能瓶颈：分析`LuaStack.cpp`中的数据交换
  - [ ] 理解当前实现：数据如何在C++和Lua之间传递
  - [ ] 进行优化：减少不必要的数据复制
  - [ ] 测试验证：测量优化前后的性能差异

## 学习资源

### 1. 源码分析
- [ ] **核心文件阅读**：按上述顺序阅读关键文件
- [ ] **代码注释**：理解关键函数和流程的注释
- [ ] **调用关系**：分析函数调用链和依赖关系

### 2. 调试工具
- [ ] **VS Code + Lua Debug**：设置Lua代码调试环境
- [ ] **UE调试器**：使用UE的调试器跟踪C++代码
- [ ] **性能分析工具**：使用UE的性能分析工具

### 3. 实践项目
- [ ] **示例项目**：运行和分析UnLua的示例项目
- [ ] **测试项目**：创建小型测试项目验证修改
- [ ] **实际项目**：在实际项目中应用修改

## 学习进度跟踪

| 阶段 | 学习内容 | 预计时间 | 完成状态 |
|------|----------|----------|----------|
| 阶段一 | 核心文件结构与初始化流程 | 3-5天 | ❌ |
| 阶段二 | 反射系统与UE交互 | 5-7天 | ❌ |
| 阶段三 | 注册表系统 | 2-3天 | ❌ |
| 阶段四 | 类型绑定系统 | 5-7天 | ❌ |
| 阶段五 | 函数与委托绑定 | 3-5天 | ❌ |
| 阶段六 | 堆栈操作与数据转换 | 3-4天 | ❌ |
| 阶段七 | 错误处理与调试 | 2-3天 | ❌ |
| 阶段八 | 配置系统 | 1-2天 | ❌ |
| 阶段九 | 扩展与自定义 | 3-5天 | ❌ |
| 阶段十 | 实战修改练习 | 7-10天 | ❌ |

## 总结

通过本学习计划的系统学习，您将能够：

1. **理解UnLua的核心架构**：掌握UnLua的整体结构和工作原理
2. **定位关键代码位置**：知道各个功能模块的实现位置
3. **进行有针对性的修改**：能够根据需求修改UnLua的行为
4. **扩展UnLua功能**：能够为UnLua添加新的功能和特性
5. **优化UnLua性能**：能够识别和解决性能瓶颈

记住，"改得动unlua"的关键在于理解其核心机制，从基础组件入手，逐步深入，通过实践不断积累经验。祝您学习顺利！
