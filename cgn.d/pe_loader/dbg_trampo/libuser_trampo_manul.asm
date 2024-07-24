; ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z

EXTERN ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z :PROC

.const
    name_func1 DB "?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z",0
    name_inc DB "?func1_inc@@YAHAEAH@Z",0
    name_arr DB "?func1_arr@@YA?AV?$vector@UMyTypeA@@V?$allocator@UMyTypeA@@@std@@@std@@PEBUMyTypeA@@@Z",0
    msvc_trampo_0 DB "?get_host_info@Tools@cgn@@SA?AUHostInfo@2@XZ",0
    msvc_trampo_1 DB "asic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V12@@std@@@2@@std@@XZ",0
    msvc_trampo_2 DB "?api@@3VCGN@cgn@@A",0
.code
    UNUSED_FUNCTION PROC
        lea rax, msvc_trampo_0
        ret
    UNUSED_FUNCTION ENDP

    ?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
        sub rsp, 30h
        mov [rsp + 10h], rcx
        mov [rsp + 18h], rdx
        mov [rsp + 20h], r8
        mov [rsp + 28h], r9
        lea rcx, name_func1
        call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
        mov rcx, [rsp + 10h]
        mov rdx, [rsp + 18h]
        mov r8,  [rsp + 20h]
        mov r9,  [rsp + 28h]
        add rsp, 30h
        jmp rax
    ?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

    ?func1_inc@@YAHAEAH@Z PROC
        sub rsp, 30h
        mov [rsp + 10h], rcx
        mov [rsp + 18h], rdx
        mov [rsp + 20h], r8
        mov [rsp + 28h], r9
        lea rcx, name_inc
        call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
        mov rcx, [rsp + 10h]
        mov rdx, [rsp + 18h]
        mov r8,  [rsp + 20h]
        mov r9,  [rsp + 28h]
        add rsp, 30h
        jmp rax
    ?func1_inc@@YAHAEAH@Z ENDP
    
    ?func1_arr@@YA?AV?$vector@UMyTypeA@@V?$allocator@UMyTypeA@@@std@@@std@@PEBUMyTypeA@@@Z PROC
        sub rsp, 30h
        mov [rsp + 10h], rcx
        mov [rsp + 18h], rdx
        mov [rsp + 20h], r8
        mov [rsp + 28h], r9
        lea rcx, name_arr
        call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
        mov rcx, [rsp + 10h]
        mov rdx, [rsp + 18h]
        mov r8,  [rsp + 20h]
        mov r9,  [rsp + 28h]
        add rsp, 30h
        jmp rax
    ?func1_arr@@YA?AV?$vector@UMyTypeA@@V?$allocator@UMyTypeA@@@std@@@std@@PEBUMyTypeA@@@Z ENDP
end
