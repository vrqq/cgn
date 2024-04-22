ninja中的 generator=1 只针对 build.ninja 这个入口文件有效 （见build.ninja）

`build build.ninja: regen build.src app1.src` 
这样写正常

`build build.ninja: regen build.src` 
若这样写，仅靠`build app1.ninja`检测更新，然后修改app1.src中的内容，则需要运行两次`ninja exe2` 才能真正执行
```bash
vrqq@rhedt ninjagen$ ninja exe2
[1/1] Regenerating ninja files
vrqq@rhedt ninjagen$ ninja exe2
[1/1] echo "World3311" > exe2
vrqq@rhedt ninjagen$
```
