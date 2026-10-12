! SPDX-License-Identifier: GPL-3.0-or-later
! The same in a recursive host: every activation has its own TOTAL, so
! one static slot for the host's frame would not do
! (tools/abi/probes/execstack.sh compiles it with lfortran -O2 -S).
recursive integer function outer(n) result(res)
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
  if (n > 0) total = outer(n - 1)
  call apply(add, n)
  res = total
contains
  integer function add(x)
    integer, intent(in) :: x
    total = total + x
    add = total
  end function add
end function outer
