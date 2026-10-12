! SPDX-License-Identifier: GPL-3.0-or-later
! A Fortran internal procedure passed to an external procedure (Fortran
! 2008): the Fortran form of a GNU C nested function whose address
! escapes. lcc's C front end has no nested functions, so this shows how
! lcc's code generator gives a callee its parent's frame
! (tools/abi/probes/execstack.sh compiles it with lfortran -O2 -S).
integer function outer(n)
  interface
    subroutine apply(fn, n)
      interface
        integer function fn(x)
          integer, intent(in) :: x
        end function fn
      end interface
      integer, intent(in) :: n
    end subroutine apply
  end interface
  integer, intent(in) :: n
  integer :: total
  total = 0
  call apply(add, n)
  outer = total
contains
  integer function add(x)
    integer, intent(in) :: x
    total = total + x
    add = total
  end function add
end function outer
