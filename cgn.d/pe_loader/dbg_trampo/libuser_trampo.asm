; ?find@GlobalSymbol@cgn@@SAPEAXPEBD@Z

.const
	name_func1 DB "?func1@@YA?AUMyTypeA@@HV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@@Z",0
	name_inc DB "?func1_inc@@YAHAEAH@Z",0
	name_arr DB "?func1_arr@@YA?AV?$vector@UMyTypeA@@V?",0
end

.code
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
