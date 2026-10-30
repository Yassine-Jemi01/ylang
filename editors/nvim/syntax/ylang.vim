if exists("b:current_syntax")
  finish
endif

syn case match

" Core declarations and control flow, including YLang 2.0 constructs.
syn keyword ylangKeyword fn function return let const if else loop while for break continue and or not await
syn keyword ylangDeclaration struct enum match from import
syn keyword ylangBoolean true false
syn keyword ylangType int float bool char string void http_task
syn keyword ylangBuiltin print

" Qualified built-in namespaces and user-defined enum/struct names.
syn match ylangNamespace "\<\(math\|string\|path\|io\|image\|gui\|http\|env\)\>\ze\."
syn match ylangDeclaredType "\<[A-Z][A-Za-z0-9_]*\>"
syn match ylangQualifiedType "\<[A-Z][A-Za-z0-9_]*\>\ze\s*\."
syn match ylangMember "\.\zs[A-Za-z_][A-Za-z0-9_]*"
syn match ylangEnumVariant "\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*[,}]"
syn match ylangField "\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*;"
syn match ylangVariable "\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*\%(=\|+=\|++\)"

" Numbers include decimal/hex integers, decimal floats, and exponents.
syn match ylangNumber "\<\%(0[xX][0-9A-Fa-f]\+\|[0-9]\+\%([.][0-9]\+\)\?\%([eE][+-]\?[0-9]\+\)\?\)\>"

syn match ylangOperator "->\|++\|+=\|==\|!=\|<=\|>=\|[=+*/%<>!-]"
syn match ylangFunction "\<[A-Za-z_][A-Za-z0-9_]*\>\ze\s*("
syn match ylangFunctionDeclaration "\<\(fn\|function\)\s\+\zs[A-Za-z_][A-Za-z0-9_]*"

syn region ylangFString start=+f"+ end=+"+ contains=ylangInterpolation,ylangEscape
syn region ylangString start=+"+ end=+"+ contains=ylangEscape
syn match ylangCharacter "'\([^'\\]\|\\.\)'"
syn region ylangInterpolation start=+{+ end=+}+ contained contains=ylangKeyword,ylangDeclaration,ylangBoolean,ylangType,ylangNumber,ylangOperator,ylangBuiltin,ylangFunction,ylangNamespace,ylangMember,ylangEscape
syn match ylangEscape /\\[nrt0x"']/ contained

syn region ylangBlockComment start="/\*" end="\*/" contains=ylangTodo
syn match ylangLineComment "//.*$" contains=ylangTodo
syn keyword ylangTodo TODO FIXME NOTE contained

hi def link ylangKeyword Keyword
hi def link ylangDeclaration Keyword
hi def link ylangBoolean Boolean
hi def link ylangType Type
hi def link ylangBuiltin Statement
hi def link ylangNamespace Include
hi def link ylangDeclaredType Type
hi def link ylangQualifiedType Type
hi def link ylangMember Identifier
hi def link ylangEnumVariant Constant
hi def link ylangField Identifier
hi def link ylangVariable Identifier
hi def link ylangNumber Number
hi def link ylangOperator Operator
hi def link ylangFunction Function
hi def link ylangFunctionDeclaration Function
hi def link ylangFString String
hi def link ylangString String
hi def link ylangCharacter Character
hi def link ylangInterpolation Special
hi def link ylangEscape SpecialChar
hi def link ylangBlockComment Comment
hi def link ylangLineComment Comment
hi def link ylangTodo Todo

let b:current_syntax = "ylang"
