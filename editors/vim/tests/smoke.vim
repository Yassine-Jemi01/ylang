set nocompatible
set nomore
execute 'set runtimepath^=' . fnameescape(getcwd() . '/editors/vim')
filetype plugin indent on
syntax on

let s:tmp = tempname() . '.yl'
call writefile(['function main() -> int {', '    return 0;', '}'], s:tmp)
execute 'edit ' . fnameescape(s:tmp)

call assert_equal('ylang', &filetype, 'Vim must detect .yl as YLang')
call assert_equal('ylang', get(b:, 'current_syntax', ''), 'YLang syntax must load')
call assert_equal(4, &l:shiftwidth, 'YLang indentation must use four spaces')
call assert_equal('// %s', &l:commentstring, 'YLang comments must use //')
call assert_equal('ylangKeyword', synIDattr(synID(1, 1, 1), 'name'), 'function keyword must be highlighted')

if len(v:errors) > 0
  for s:error in v:errors
    echomsg s:error
  endfor
  call delete(s:tmp)
  cquit
endif

call delete(s:tmp)
qa!
