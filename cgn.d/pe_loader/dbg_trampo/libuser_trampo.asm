; ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z

.const
	name_func1 DB "?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z",0
	name_inc DB "?func1_inc@@YAHAEAH@Z",0
	name_arr DB "?func1_arr@@YA?AV?$vector@UMyTypeA@@V?",0

	msvc_trampo_0DB "?get_host_info@Tools@cgn@@SA?AUHostInfo@2@XZ",0
	msvc_trampo_1DB "asic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@2@V?$allocator@U?$pair@$$CBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@V12@@std@@@2@@std@@XZ",0
	msvc_trampo_2DB "?api@@3VCGN@cgn@@A",0
end

.code
	UNUSED_FUNCTION PROC
		mov rax, msvc_trampo_0DB
		ret
	UNUSED_FUNCTION ENDP

	?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z PROC
		push name_func1
		call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
		add rsp, 4
		jmp rax
	?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z ENDP

	?func1_inc@@YAHAEAH@Z PROC
		push name_inc
		call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
		add rsp, 4
		jmp rax
	?func1_inc@@YAHAEAH@Z ENDP

	?func1_arr@@YA?AV?$vector@UMyTypeA@@V? PROC
		push name_arr
		call ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z
		add rsp, 4
		jmp rax
	?func1_arr@@YA?AV?$vector@UMyTypeA@@V? ENDP
end
