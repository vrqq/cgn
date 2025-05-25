If you see the error like
```
perl.h(1781): fatal error C1083: Cannot open include file: 'arpa/inet.h': No such file or directory
NMAKE : fatal error U1077: 'cl.exe -c -Imini /utf-8 /wd4828 /EHsc /MD -DPERL_EXTERNAL_GLOB -DPERL_IS_MINIPERL -Fo.\mini\av.obj ..\av.c' : return code '0x2'
Stop.
ninja: build stopped: subcommand failed.
```
It caused by override cflags written by Makefile, please add '/I.' in cflags.
