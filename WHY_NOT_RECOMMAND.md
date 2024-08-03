## Why Not recommand
这是一个比较失败的项目: 无论gcc还是clang 编译dll的速度实在太慢(0.5秒)，若一个小型项目(几十个BUILD文件) 每次修改几个 都需要数秒analyze，首次生成甚至需要数十秒analyze 很难接受。几个改进方向:  
* 增加一个 test-build 在首次运行时尝试编译所有扫描到的cgn-script
* JIT: llvm IR
* PCH: instantiate member function 
* 去C++ STL / 二次封装 减少模板解析时间
* 保留其思想 使用python/js/starlark重写

**原因猜测**
以 `//hello:demo` 为例，使用 `clang++ -ftime-trace` 分析，单文件最快能到100ms
* compiler - frontend 190ms
    * 其中 debug信息 55ms 可省略
    * 其中模板展开 `unordered_set<string> / vector<string>` 等共90ms
* compiler - backend 20ms
* 其他耗时? 60ms
    * 或许使用任意c++ JIT (LLVM-IR / mmap + asm / ...) 能省掉

或许C++并不适合，c++和JIT语言的编译器发展方向不是一个思路，对于c++ 前端慢也不是问题。

* cpp编译器认为的快 = 编译后的程序运行快
* JIT语言认为的快(例如python) = frontend+运行 总体快

也就是说, GO Rust 什么的都不适合做编译脚本语言，因为他们的编译器 都不把frontent快 当首要目标，并且认为frontend的速度可以为了 代码优化而牺牲一些

之前用过腾讯的blade-build ( https://github.com/chen3feng/blade-build/ )，基于python的 我在很久之前试过，analyze慢，我想这也是bazel要推starlark的原因。(starlark = 精简版python)

用过了bazel / buck我觉得都有问题：舍弃了类似gn的just-in-time-analyze，使用一大堆interface约定整个buildtools的代码流程，创造了一堆概念，语言越写越乱。这也是我开这个project的原因：和GN一样, `TargetDeclaration => Interpreter => 生成ninja文件`, 只不过其中每一步都变成了`c++ function`，gn是"不完全的function"。

回到GN，再向上扩展 或许作者也考虑到 语法解释器迅速膨胀，最后扩展成新的一门语言，那不如基于python或其他脚本语言重写框架。同时向上扩展又面临算力有限 frontend速度慢（像这个项目一样 无法被优化），想获得舒服的全面语法 就承受缓慢的frontend。

而Bazel像是从cmake进化来的，调用函数完成定义，把cmake的 单个函数单个作用 变成了 单个函数多个入参控制。
