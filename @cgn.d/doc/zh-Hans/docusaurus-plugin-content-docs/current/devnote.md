# 开发笔记

## 多线程并行问题几则
要求线程安全毋庸置疑

**API.analyse_target() 允许多线程?**
假设有如下target依赖链 `A->B->C->A`(循环引用 invalid) 和 `D->C`, 线程1访问 `analyse_target(A->B->C)`, 线程2访问 `analyse_target(D->C->A)`.
起初想`analyse_target()`也可以并行, 后来有向图多线程环检测没想好, 索性暂时recursive lock, 这个函数仅单线程访问, 退而求其次 `active_script()` 多线程预加载.


## NamedConfig在依赖图中以点存在
这来源于20240818 处理的一个corner-case. 起初在protobuf() interpreter里面发现的, 里面调用了 `analyze_target(query_config("host_release"))`. 然后我改了host_release的定义, 尝试运行 发现没有re-analyse.
因为adep的file[]并不能体现`cfg_name->cfg_hash_id`的对应关系, 这里面确实无法触发re-analyse.

如何把config_name和config_hash_id对应关系加到某个Node下呢?
比如protobuf() 这个rule使用了 `api.query_config()` 那么我就需要在这行调用下面加上 `graph.add_adep_edge()`

Solution: 虚构Graph中的一个点 例如 `C-FFFF9843-DEFAULT` 表示name和ID的对应关系, 在任何地方只要出现`query_config()`获取 Configuration* 就需要增加这条依赖关系.
按名字索引本意是表示依赖某个名字 而不是后面的CfgID

## API.offline_script() 的演化过程
一个常见场景: CGN后台常驻, 用户修改了一些BUILD.cgn.cc 然后调用 cgn build 编译某个target. 此时CGN内部, 先调用`start_new_round()` 将所有GraphNode的 "文件mtime更新标记" 设为unknown, 后又根据GraphNode内部记录的文件列表 发现了相应文件被改了, 进而自动 `API.scripts[].erase()`(dll detach) 然后再 `active_script()`, 从而保证结果正确.

起初设计`offline_script()` 只完成dll detach, 以便用户能从cgn-out文件夹将dll删掉, 没有必要从内存和数据库中移除相应GraphNode. GraphNode仅仅记录了文件列表 不存在对DllHandle的引用. 若对应的script文件没了, 这个GraphNode在数据库里就成为一个垃圾数据, 赢在未来某个时候移除掉. 若对应的script文件mtime没变, GraphNode还有可能减少一次编译.
另外一个不移除点的原因, 移除某个GraphNode 同时也应移除所有后续节点, 在一个正常的依赖链中, 去掉一个点 会在之后 `analyse_target()` 时返回false, 从而重新生成一个没有依赖边的新图.

`offline_script()` 若是仅包括dll detach, 作为单独的API太单薄, 还要在API增加额外的`set_to_unknown()`接口, 对用户来说才能完整的干完一个有意义的事情: 手工删掉dll. 故这个API改为了 dll detach 和 set GraphNode to unknown 二合一.

## cgn-out/configuration 是否需要防呆设计?
若用户删了某个配置文件: 可能同hash的配置 不能定位到正确的hash上.
若用户修改了某个配置文件: ???

## Ninja Order-only dependencies 到底是什么
我们有如下build.ninja
```ninja
build b.o : cxx b.cpp
build a.o : cxx a.cpp
```
假设我们每次编译 都要在命令行中按顺序执行: `ninja b.o` `ninja a.o`, 那么 我们就可以改写以上ninja为
```
build b.o : cxx b.cpp
build a.o : cxx a.cpp || b.o
```
这样, 我们就可以只运行 `ninja a.o`, 就可以触发 ninja内部先自动执行 `ninja b.o`, 然后才真正运行 `ninja a.o`.

换句话说 `a.o` 和 `b.o` 是**两个独立的target**, 只是我们不想每次打两条ninja编译命令, 而想**用一条ninja命令 依次触发两个target的编译**.

符合官方文档对order-only的解释. 我们在此拆解下官方文档
> *Order-only dependencies, expressed with the syntax || dep1 dep2 on the end of a build line.*
> *When these are out of date,* : **this** means "|| dep1 dep2"
> *the output is not rebuilt until they are built,*: **output** means "build xxx"
> *but changes in order-only dependencies alone do not cause the output to be rebuilt.*: **changes in order-only dependencies** means "|| dep1 dep2"

在ninja中 `build file1: || file2` 依赖, 这种被称为Order-only dependencies, 表示file2 一定要在file1之前完成执行, 换句话说file1 依赖的是file2的副产物, 并通过/showInclude或dyndep等办法, 而非implicit input 引入依赖.
详见 https://blog.vrqq.org/archives/1005/

**cmake_configuration()的思考**
* `CxxInfo.include_dir` 可以有 repo/include 亦可有 out_dir/build/include, 要求使用者至少以 "order_only_dependency"形式在ninja中引入, 才能形成正确的依赖关系.
* "xx.so" 来自 `LinkAndRunInfo`, 无需 order_only_dependency 下游就能正常形成依赖关系

**cxx_sources()思考**
* `CxxInfo` 提供cflags.include_dir 下游无需order_only_dependency 就能正确依赖.
* `LinkAndRunInfo` 提供 .o文件, 下游也无需order_only_dependency 就能正确依赖.

**因此**
例如`cxx_sources()`, `cxx_precompile()` 等等, 通过return InfoTable[] 中的"文件列表"就可以形成正确依赖的, 就不要求下游使用order_only_dependency. 
"文件列表"指例如`LinkAndRunInfo.shared_files[]`, 下游通常直接把文件加入 ninja target input[].

而例如 `cmake()`, `protobuf()` 等等, 产生的header不在文件列表中 (不要求CxxInfo.include_dir 在ninja target中作为 implicit input), 故无法形成正确依赖, 因此需要下游在编译时 将其作为order_only_dependency引入.

以上, 为了在ninja file中形成正确的关系, 我们约定:
一个上游CGNTarget 作为provider, 在不强制下游的ninja_target 以 "order_only_dependency形式" 引入 "上游的ninja-target.stamp" 时, 下游的ninja_target 能否完整的使用上游CGNTarget 提供的全部信息.
我们在CGNTarget返回值中新增 `ninja_dep_level = FULL, DYNHDR, NONEED` 表示**建议**下游生成的ninja-target以何种程度引入上游信息(具体怎么用由下游interpreter决定), 分别为
* FULL: 建议下游 正常的利用上游返回InfoTable[]的方式时, 需以 implicit input 引入上游的.stamp, 即上游有更改, 下游必须重新编译. 至于下游内部如何FULL, 影响到啥程度, 由下游 interpreter 自行决定. 此选项目前暂无应用场景.
* DYNHDR: 下游通过 ninja-dyndep或 通过/showIncludes 动态引入依赖, 借助ninja提供的功能, 默认场景.
* NONEED: 见下文cxx_sources()样例

**NONEED场景**: 上游是cxx_sources(), 返回了 "已经存在的`include_dir[]`文件夹" 和 "`xx.o`编译结果", 下游build.ninja即便不写 `build downstream: || upstream/.stamp`, 也可使用 "上游cxx_sources()" 返回的全部信息.
**DYNHDR场景**: 上游是protobuf(), 其提供的`include_dir[]`需要在编译时生成, 下游只有在写了"order_only_dependency" 才能通过`/showInclude`形成正确的ninja 依赖.

**pub_dep例子**: 若在`cxx_sources()`里面以`add_dep(cxx::inherit)`引入了一个"给出DYNHDR的target", 表示的是当前`cxx_sources()` 代替依赖行使提供DYNHDR的权利义务, 故也应至少返回DYNHDR建议它的下游.

## struct CGNTarget的思考
CGNTarget 是 `analyse_target()`的返回结果

**no_cache/no_store**
决定了在每轮analysis_round中, 对相同入参的 `analyse_target(factory_label, cfg)` 的返回值是否缓存.
考虑一种情况:
```cpp
cxx_sources("myexe", x) {
    x.defines["VAR1"] = rand();
}
```
看上去应该每次依赖到它时都应 analyse()一边, 但他输出的文件夹是以cfg[]决定的. 而要interpreter支持这种情况, 一种做法是在每次analyse时都新建一个子文件夹, 生成一个不同于以往的build.ninja, 这显然有些浪费. 且增加了interpreter的复杂度.

因此, 在没有想到场景时, 暂时去掉这个flag.

**cpp __DATA__**
这个和CGNTarget缓存无关, 仅在CxxInfo中 指出哪个文件需要重新编译即可. (TODO 目前暂无这个选项)

## TargetFactory 的注册和调用细节

在每个BUILD.cgn.cc中, 会有一些用户写的规则, 其用户写法和展开后实际情况如下
```cpp
// 用户写法
cxx_sources("xyz", x) {}

// 展开后
shared_ptr<void> factory_1234 = api.bind_factory<CxxInterpreter>(&factory_1234_fn);
void factory_1234_fn(string name, cxx::CxxContext x) {}
```
当BUILD.cgn.cc所编译的DLL dlopen进cgn主程序时, 通过全局变量赋值, 将`factory_1234_fn`注册到cgn内部. API返回的`shared_ptr`的析构函数也可以保证 在dlclose时 自动在cgn内部移除相应函数指针.

在cgn内部, `api.bind_factory<TypeInterpreter>(fn)` 做了如下事情
```cpp
// 0. 每次cgn调起用户factory时, 会先准备CGNTargetBuilderIn, 其中包含此次调用的configuration
lambda_function = [](CGNTargetOptIn* opt)
{
    // 1. 根据Interpreter header 中的要求, 先挂载interperter的依赖
    api.load_script(TypeInterpreter::preload_labels());

    // 2. 生成Interpreter::Context 给用户的factory调一下
    TypeInterpreter::context_type ctx{opt};
    fn(ctx);

    // 3. 把填好结果的Context 喂给Interpreter执行
    TypeInterpreter::interpret(ctx);
}
api.bind_target_builder("label", fn);
```

## 当analysis_target()支持 configuration trim后的情形
在之前的版本, 每个target的config传入时就已确定, 哪怕里面有众多不相干的entry, 例如 `cxx_sources()` 会被 `cfg["PYTHON_XXX"]` 影响.
之前可以借鉴ninja的优化策略, 若在analyse某个target前, 检查其依赖链的每一个点 mtime均无变化, 则当前target可以认为是最新的, 就不用重新执行analyse过程(免去dlopen等).

在当前版本或许不行了, GraphNode通常是在interpreter内部 `opt.confirm()` 时才确立的, 也就是说不走完全部analyse过程, 都拿不到config_id 同样的也不知道其输出文件夹.
暂时想到两个地方:
* 缓存初始的config_id 和 trimmed_config_id 对应关系, 可以建立虚拟 GraphNode 指向真正的GraphNode 来落地, 代价是增加数据库尺寸.
* 缓存`pair{label, trimmed_cfg}`组合, 在 `opt.confirm()`时, 若有 直接返回上次的结果. 代价是走完analyse过程, 但省去了NinjaFile* fileIO.

两者可以兼用, 但目前在未知性能的前提下, 暂时选择只准备后者, 一般磁盘IO才是速度缓慢根源.

## 源码和cgn-out跨磁盘的思考
在build.ninja中, 所有相对路径都以执行ninja命令的CWD为准. 一般我们把CWD称之为WorkingRoot, 且cgn-out也在WorkingRoot下.
若源码在类似samba共享的文件夹下, windows就不宜直接把cgn-out写进samba共享(ninjabuild或者samba有bug 经常无法写入文件), 故把cgn-out放在本地磁盘上. 此时在各个build.ninja中就面临抉择, 源码和输出一定不在一个磁盘, 只能一个使用相对CWD (WorkingRoot)路径, 一个使用绝对路径.
思前想后, 我们选择源码使用相对WorkingRoot的路径, 而输出(cgn-out) 使用绝对路径. 好处是可以把源码挪位置, 代价是不能把cgn-out文件夹挪位置.

## class Graph 数据库接口及伪代码设计
存储analysis graph的文件, 仿照 ninja的数据库, 文件格式如下
`<1024 bytes file signature> + <64 bytes version> + <block> + <block>`
其中block分为3类, 具体格式参见 graph.h
* case 1: (StringBlock) string + int64_t
* case 2: empty block
* case 3: (GraphNode) title + file_list[] + inbound_edge_list[]

**细节, 若GraphNode.files[] 中某个文件记录的 mtime=0 和 该文件不存在, 对应当前节点的status?**
如果files[]中某个文件不存在, 当前节点标记为Stale. 看似和 mtime=0 对应, 实际考虑一种情况:
某个.so文件对应的源文件没了, 按理说应当重新编译.

**Graph::set_node_files[] 之后, 该点状态应该是什么?**
Graph::Latest 表示: 文件列表的mtime 实际和数据库记录的值一致
也就是说 Graph仅负责 "文件列表的mtime -> 点状态" 维护, 至于内容是否合乎逻辑, 其无法考证, 而应由使用者(CGNImpl)负责.
故修改文件列表后, 将节点状态设置为`Unknown`即可.

**`GraphNode::file[0]`什么作用**
在graph.cpp中, 有如下代码 `is_stale |= (p->files[0]->mem_mtime < p->max_mtime)` 我们默认`file[0]`为输出文件, 要求其文件时间戳最大, 否则认为该节点stale. 在CGNScript中有点作用, 但在CGNTarget中 `GraphNode::file[]` 就仅包含当前target的 build.ninja 文件了.

**对外接口和impl**
```cpp
get_string(s) {
    return db_strings[s] if exist;
    db_strings[s] = ofstream::write(s);
}
get_node(title) {    
}
set_node_default_state(node) {}

```


## AnalysisGraph的状态更新过程
起初, 所有的Node都是Unknown的, 整个Graph的规则如下:
* 若有两个点U和V 且有一条边 U->V  == "V依赖U"
* Unknown 和 Stale 点, 可以依赖任何状态的点
* Latest点, 其依赖及递归的依赖, 均只能为Latest

`class Graph`提供了一些函数, 用以修改node状态, 于是我们起初设计了如下函数:
* `test_status()` 如果当前点是unknown, 根据其依赖和自身的files[] mtime更新情况, determinate it 到 Stale 或 Latest 状态.
* `set_node_status_to_unknown()` 将Stale 或 Latest的node 设置为Unknwon, 同时为了确保图合理, 将递归设置其依赖的状态
    * 若 Unknown -> Unknown : return
    * 若 Latest -> Unknown: 正向dfs该点, 后面的一串若有Latest 都修改为Unknown
    * 若 Stale -> Unknown: 直接修改即可
* `set_node_status_to_latest()`
    * assert(检查其所有依赖点 是否均为Latest)
    * 将当前点所列文件 数据库里面的mtime更新到当前磁盘的mtime值, 然后设置为Latest, 可以从任何状态转移到Latest
* `set_node_status_to_stale()`
    * Stale -> Stale : return
    * Unknown or Latest -> Stale : 正向dfs其后续的链上所有点均设置为Stale. (解释: 有一条边U->V 程序检查U时 发现需要更新 先设置为Stale, 然后重新编译U, 然后设置U到Latest. 接下来程序再检查V时 理应重新编译V, 但发现U是Latest. )

值得注意的是 NodeStatus不落盘 仅存储在数据库里面.

**一个错误优化: 起初, 我考虑了_forward_status(), 但这和 test_status() 是冲突的**
每次修改点状态时, 仅修改当前点和其一层dfs(), 详情如下:
* 若当前点是unknown 检查其下一层点如果有Latest 设置为unknown
* 若当前点是stale, 设置其下一层所有点为stale
考虑一个比较长的图, 祖先点Stale后 hot-reload, 后面依赖他的点均应该先设置为Stale并重新触发编译检查. 如果仅设置一层, 比较晚辈的点仍然可能是Latest

**对于Named-config点的tricky**
按照我们的设计, 表示 "ConfigName和ConfigID对应关系" 的点, 其初始状态为Stale, 初始化ConfigMgr时如果发现该对应关系没变, 则把这个点设置为Latest(), 否则置之不理. (因为它没有file[] 可用)
设置初始值时不能用`set_node_status_to_stale()`, 因为我们本意他是个 undetermined state, 我们不希望递延它的状态.

另一个方案, 或许更好理解 `ConfigNode.files[] = cgn-out/configuration/FFFE1122.cfg`, graph新增遍历接口, 在CfgMgr的初始化过程中 遍历所有的Config点设置其初始状态为stale. 无用接口和多余的遍历比较浪费, 故放弃这个方案.


## 以protobuf为例 如何处理复杂的interperter
`@third_party//protobuf/protoc.cgn.cc` 例如有一文件 a.proto 其经过protoc生成 a.pb.h 和 a.pb.cc, 我们想通过`rule protobuf()`向外提供 `cxx_sources()` 相同的返回值. 若我们将.pb.cc文件输出到cgn-out, 就需要先`opt.confirm()`确定输出目录, 然后填充 `CxxContext.pub.include_dirs[]`, 然后再调用`CxxInterpreter`生成返回值. 可是在`CxxInterpreter`内部也需要`opt.confirm()`, 显然这两次`confirm()`得到的结果不一样, 考虑通用性 我们也不能要求其一样, 于是有如下2种方案.

**Case1: CGN_RULE_MARCO**
BUILD.cgn.cc
```cpp
std::vector<std::shared_ptr<void>> factory_stub_list = [](){
    stub1 = api.bind_target_factory<CxxInterpreter>(name + ".1", [](CxxContext &x){});
    stub2 = api.bind_target_factory<ShellInterpreter>(name + ".2", [](ShellContext &x){});
    return {stub1, stub2};
}();
```

**Case2: sub_target**
创建sub_target, 例如 `//hello/tgt1` 的原始输出文件夹为 `cgn-out/obj/hello_/tgt1_<CfgID>`, 其subtarget的输出文件夹为`cgn-out/obj/hello_/tgt1_<CfgID>/<subname>_<subCfgID>`. subtarget可以嵌套, 即sub-sub-target为`cgn-out/obj/hello_/tgt1_<tgt1_CfgID>/<subname>_<subCfgID>/<subsub>_<subsubCfgID>`
```
CGNTargetOptIn *sub_opt = opt.create_sub_target(CGNTargetOptOptIn *current_opt);
```
然后在主target 的interpreter结束, 返回到`analyse_target()`时, 自动更新主target及所有subtarget的ninjafile 和 GraphNode.  
为了简化GraphNode的依赖关系, 我们默认AnalyseGraph为: MainTarget -> subTarget -> subsubTarget ...  
并且对于每个边, 在上层target的 interpreter内部 创建subtarget时, 指定当前subtarget是否为真实的`CGNTarget result`.

**为何不能创建anonymous target**
最终编译还是由ninja执行, 其依赖于解析 build.ninja 文件, 而 anonymous target 不知道将该文件放在哪里.  

**思考 interpreter的上下文是什么**
用户代码的interpreter, 想要的是得到当前factory label + name, src_dir, cfg, 以及在confirm之后获得 out_dir.  
因此面对interpreter来说, subtarget和普通的target, 仅cfg和out_dir不同, 进而ninjafile也不同.
在内部: 因cfg不同 故anode也不同, result_cache_label也不同. (允许cache)

**思考 为何不使用api.create_sub_target()**
考虑如下代码:
```cpp
ProtobufInterpreter("myproto", x) {
    if (x.cfg["optimization"] == "debug")
        api.create_sub_target("same_name");
    else
        api.create_sub_target("same_name");
}
```
当`analyse("myproto", cfg=[debug])`及`analyse("myproto", cfg=[release])`同时访问时, 他们会输出到同一个文件夹. 故要求先confirm在创建subtarget. 新的subtarget中, name, label, src_prefix不变, 其余变化.
原GraphNode名称为"T + factory_label + ConfigID" 因增加subconfig, 现改为

**思考 subtarget和其上级target能否共用`GraphNode *anode`**
不能. 以protobuf为例, 其生成的GraphNode为`T//hello/proto#00000000`, 而其在debug和release模式下, subtarget应指向不同的点.

**结论**: 考虑方案2并修改`struct CGNTargetOpt`, 增加创建subtarget的函数, 在创建时就指定当前subtarget是否为result, 且subtarget之间无法互相依赖, 仅能subTarget依赖mainTarget.  
(虽然逻辑上不完整 但暂时解决了问题 以后有需求在考虑更新这个设计)


## 目前尚未解决的语义问题 `cfg[“host_os”]`
以`url_download()`为例, 当我们 同一个workingRoot目录 被不同host (windows、linux等) 以共享文件夹挂载时, 我们希望git() 下载的第三方源码 直接下载到 workingRoot里面, 而不是 cgn-out里面.  
同一个输出文件夹 `xxx\repo` 只能对应一个build.ninja下的taget, 可linux(curl)和windows(certutil)的下载命令不同.  
可能的解决方案: 对于不同host_os生成不同的build.ninja, 在执行命令时检测sha1, 使用下文`copy()`提案.  

对于git() 也可考虑使用其他的depot工具例如libgit2.  

类似的问题 对于c++ with clang compiler, host_os不同 其参数格式也不同, 故尔也不应该是同一个 build.ninja. 不过这个可以通过`cfg["host_os"]`解决而不增加资源浪费.

**NinjaBuild的思考**
以上只解决了一小部分, ninja build 到底代表了什么?
```ninja
rule
    depfile = target.d
build output: rule input
build dummyfile : phony
```
在这个例子中
* `output`代表全局的 当前target的调用点, 可以让其他target加为引用使其build.
* `input` + `output` + `target.d` 代表当前target关心mtime的文件列表.
* `dummyfile` 代表在ninja分析期间尚不存在的文件 可能有其他target生成

**CGN的问题**
在上述ninja例子中, 同一个`dummyfile`可能由多个 BUILD.cgn.cc 生成, 一旦生成便写入了全局的 main.ninja  
再以后, 若 BUILD.cgn.cc 迁移, 并不会导致之前生成的 build.ninja 被移除, 这可能存在潜在的冲突.  
若文件在cgn-out下, 因目录有自己的mangle系统, 可解决此问题. 但若输出文件直接在源码目录中, 则该问题存在.
例如:
1. p1/common/BUILD.cgn.cc 生成了某个protobuf 中间文件 : p1/common/quick_comm.pb.cc
2. 将 p1/common/BUILD.cgn.cc 并入 p1/BUILD.cgn.cc, 生成同样的 p1/common/quick_comm.pb.cc
3. 此时在 cgn-out中, 有两个target 输出相同的 p1/common/quick_comm.pb.cc 冲突. 而且 quick_comm.pb.cc 需要生成 quic_comm.pb.o 故他一定位于output.

一个暂行解决方案:   
若某个interpreter有不在cgn-out/target_out下的输出, 使用dummyfile, 而不是 target output 表示, 同时在CGN中该target return要求上层使用者ORDER_ONLY_DEP之.  

尚未确定: 以何种方式 记录 dummyfile 来自于哪个 build.ninja / BUILD.cgn.cc 可以实时检查上述问题?  

TODO: protobuf() 以及 git() 需要更新

## copy() 提案
prerequisite: 在BUILD.cgn.cc中, 我们无法提前得知输出到cgn-out下哪个文件夹.

它要做的事情:
* copy from workingRoot to cgn-out (cgn-out在其他磁盘) 用于windows下编译perl.exe
    * `result.output` 设置为 文件夹or所有文件 用户手动指定
* collect to .output from workingRoot or cgn-out without copy any file 用于给未来的 `ssh()` 准备素材
    * `result.output` 直接记录原始路径
* copy from workingRoot to workingRoot 用于一些灵活准备

现在的问题
* 和 git, url_download 这些面临同样问题, 在不同的host_os上 有不同的copy.exe, 可BUILD.ninja要监测输出的target变化来决定是否运行copy.
* BUILD.ninja中, 同一个文件 只能对应一个target的output, 同一个文件可以当作很多input 但他必须存在 或 是某个target的output.

**文件夹的mtime**
该文件夹下新增、删除了文件/文件夹, 会使当前文件夹的mtime改变. 该文件夹下的文件改变, 不会使当前文件夹的mtime变化. 该文件夹的子文件夹下新增了文件, 也不会使该文件夹mtime变化.

**考虑一种备选方案**, 以copy举例, copy的输出 在 build.ninja 的target里写为输入, 输出stamp文件(输出无意义), ninja文档如下.
> phony can also be used to create dummy targets for files which may not exist at build time. If a phony build statement is written without any dependencies, the target will be considered out of date if it does not exist. Without a phony build statement, Ninja will report an error if the file does not exist and is required by the build.

```ninja
rule write
    command = (echo IN > $in) && (echo OUT > $out)

build y.stamp : write x.txt
build x.txt : phony
```

这种方案难以对protobuf的输出自发形成依赖, 以下考虑多个target都将myproto.pb.h 和 myproto.pb.cc作为input, 仅有phony作output. 不能在ninja文件内部定位到正确的输出myproto.pb.cc的那个target.  
但可以迂回曲折 将myproto.pb.cc 以 (header) ninja_dep_order_only的形式, 传给 cxx_sources() 生成的那个target, 例子如下
```ninja

build src/pbout/myproto.pb.cc src/pbout/myproto.pb.h : phony

build out/obj/hostwin_tgtarm/myproto.stamp : run | src/myproto.proto src/pbout/myproto.pb.cc src/pbout/myproto.pb.h
    cmd = out/obj/win_hostrel/protoc src/myproto.proto --cpp_out=src/pbout/
build out/obj/hostwin_tgtarm/myproto.o : cxx src/pbout/myproto.pb.cc | out/obj/hostwin_tgtarm/myproto.stamp

build out/obj/hostlinux_tgtmips/myproto.stamp : run | src/myproto.proto src/pbout/myproto.pb.cc src/pbout/myproto.pb.h
    cmd = out/obj/linux_hostrel/protoc src/myproto.proto --cpp_out=src/pbout/
build out/obj/hostlinux_tgtmips/myproto.o : cxx src/pbout/myproto.pb.cc | out/obj/hostlinux_tgtmips/myproto.stamp

default out/obj/hostlinux_tgtmips/myproto.o
```
以上方案相当于两套不同平台的target都把 src/pbout/myproto.pb.cc 当作输出

**第二种备选方案是**, 每次调用cgn时写config.ninja 内置诸多用于redirection的变量

**下一种备选方案是**, 通过dyndep重定向到 当前cfg_id目录下的 真实target

**第一种方案的改良 高级copy**

@cgn.d/utility/advcopy -MD cpresult.d flat_copy argfile.txt

```ninja
rule advcopy
    cmd = advcopy -MF ${out}.d -stamp ${out} ${args} ${in}
    depfile = ${out}.d
    deps = gcc

build cgn-out/.stamp : advcopy argfile.txt
    args = flat_copy
```

file '.stamp.d'
```makefile
.stamp.d : all_files_in_dstdir, all_files_in_srcdir
```
若原地址中含有‘*’, 例如 `a/*/b/c/**.h`, 则将 选中的文件在src中的位置 和 文件夹a 和 文件夹c 以及 文件夹c下所有含有.h的子文件夹(递归), 全部列入依赖.  
stamp文件无意义, 因为ninja会自动删除depfile 所以不能用.d当stamp文件

**对advcopy的source pattern match的设计**
由于深层文件的mtime更改 无法影响上层文件夹, 故当需要复制某个文件夹时, 一定需要将该文件夹及其子文件夹全部加入watch.depfile, 因此, 当最后一个section为‘*’时, 等同于‘**’.

支持的规则: 每个section仅能最多有一个‘*’, 只有最后一个section支持含有‘**’表示匹配子文件夹.
例如我们设计了如下文件列表 (@cgn.d/advcopy/testcase)
```txt
>> aa
   |>> bb
       |-- fileUV.in
       |-- fileX.txt
       |-- fileX.txt.2.0 (to fileX.txt)
       |-- fileY.txt
   |>> cc
   |-- parent_link (to ..)
   |-- file_in_aa.txt
   |-- fileZ.in
   |-- invalid_link (to invalid_link)
```

**symlink处理**
首先symlink不能跳过, 也要复制. 而且symlink并不能视为普通文件.  
我们的`file_match`只返回 regular_file, dictionary, symlink三种, 其余类型忽略.(例如pipe unix_socket dev等)  
前两种检查mtime, 而对于symlink, `read_symlink()`检查其“内容”(指向) 是否变化 以决定是否重建, 若不检查直接重建symlink 可能会使得symlink本身的mtime改变, 不确定ninja是否会检查它.  
symlink会出现在`file_need_copy`及`node_need_watch`中, 我们期望ninja可以检查好 `symlink本身` 的变化.