# SPDX-License-Identifier: GPL-3.0-or-later
#
# Procedures shared by relcc2's E2K DejaGnu boards: e2k-qemu.exp runs test
# programs under MCST's qemu-e2k user mode, e2k-ssh.exp (and the machine
# boards e2k-4c.exp, e2k-8c2.exp, e2k-2c3.exp built on it) runs them on an
# Elbrus machine over SSH. This file is not a board; the boards source it.
# See ../README.md for the settings.
#
# Every board setting comes from, in this order: a Tcl variable of the
# same name (set in site.exp or as "runtest NAME=value"), the environment,
# the board's default.
#
# How a program runs: a small sh script runs it under timeout(1) with
# stdout and stderr merged, then prints "\n@@e2k-status N\n", where N is
# the exit status as sh reports it (128 + S for signal S, 124 for a
# timeout). The board strips that line from the output, so tests see only
# what the program printed, and keeps a description of the status in the
# global e2k_last_status ("exit 3", "signal 6", "timeout") for the log
# and for the smoke suite.

set e2k_boards_dir [file dirname [file normalize [info script]]]
# tools/dejagnu/boards -> the relcc2 checkout.
set e2k_repo_root [file dirname [file dirname [file dirname $e2k_boards_dir]]]

# Output kept per run; a program that prints more (a runaway loop) keeps
# its head and its last bytes.
set e2k_output_limit [expr {4 << 20}]

set e2k_last_status ""

# The value of setting NAME, or DEFAULT.
proc e2k_setting {name {default ""}} {
    if {[info exists ::$name] && [set ::$name] ne ""} {
	return [set ::$name]
    }
    if {[info exists ::env($name)] && $::env($name) ne ""} {
	return $::env($name)
    }
    return $default
}

# The lcc 1.29.16 cross compiler for an lcc target directory name such as
# e2k-v3 or e2k-v5.8c2 (one per target, chosen by directory).
proc e2k_lcc_dir {target} {
    return "/opt/mcst/lcc-1.29.16.$target.linux-6.1"
}

# Quote S for a POSIX shell.
proc e2k_shq {s} {
    return "'[string map {' '\\''} $s]'"
}

# The execution time limit in seconds for one test program: GCC's
# timeout_value (gcc,timeout, dg-timeout, dg-timeout-factor) when GCC's
# testsuite is running, else the DejaGnu variable test_timeout, else the
# board's gcc,timeout, else 300.
proc e2k_exec_limit {dest} {
    set t 0
    if {[info procs timeout_value] ne ""} {
	set t [timeout_value]
    } elseif {[info exists ::test_timeout]} {
	set t $::test_timeout
    } elseif {[board_info $dest exists gcc,timeout]} {
	set t [board_info $dest gcc,timeout]
    }
    if {![string is integer -strict $t] || $t <= 0} {
	set t 300
    }
    return $t
}

# Called by the pipe reader below.
proc e2k_io_read {chan} {
    if {[catch {read $chan} data]} {
	set ::e2k_io($chan,state) eof
	return
    }
    set head [string length $::e2k_io($chan,out)]
    if {$head < $::e2k_output_limit} {
	append ::e2k_io($chan,out) $data
    } else {
	set ::e2k_io($chan,cut) 1
	append ::e2k_io($chan,tail) $data
	set ::e2k_io($chan,tail) [string range $::e2k_io($chan,tail) end-4095 end]
    }
    if {[eof $chan]} {
	set ::e2k_io($chan,state) eof
    }
}

# Run CMD (a list: program and arguments, no shell) on the build machine
# with stdin from INP ("" for /dev/null) and stdout and stderr merged, for
# at most LIMIT seconds. Returns {state output}, state "eof" when the
# command closed its output, "timeout" when it was killed, "error" when it
# could not start. Plain Tcl pipes (no pty), so the output is exactly the
# program's bytes, as in DejaGnu's native unix_load.
proc e2k_pipe_run {cmd inp limit} {
    if {$inp eq ""} {
	set inp /dev/null
    }
    verbose "e2k: running $cmd < $inp (limit ${limit}s)" 3
    if {[catch {open "|[list {*}$cmd < $inp 2>@1]" r} chan]} {
	return [list error $chan]
    }
    fconfigure $chan -blocking 0 -buffering none -translation lf
    set ::e2k_io($chan,out) ""
    set ::e2k_io($chan,tail) ""
    set ::e2k_io($chan,cut) 0
    set ::e2k_io($chan,state) running
    fileevent $chan readable [list e2k_io_read $chan]
    set timer [after [expr {$limit * 1000}] \
		   [list set ::e2k_io($chan,state) timeout]]
    while {$::e2k_io($chan,state) eq "running"} {
	vwait ::e2k_io($chan,state)
    }
    after cancel $timer
    fileevent $chan readable {}
    set state $::e2k_io($chan,state)
    if {$state eq "timeout"} {
	catch {exec kill -KILL {*}[pid $chan]}
    } else {
	catch {fconfigure $chan -blocking 1}
    }
    catch {close $chan}
    set out $::e2k_io($chan,out)
    if {$::e2k_io($chan,cut)} {
	append out "\n\[e2k board: output cut at $::e2k_output_limit bytes\]\n"
	append out $::e2k_io($chan,tail)
    }
    array unset ::e2k_io $chan,*
    return [list $state $out]
}

# Split the status line off OUTPUT. Returns {status output}; status is -1
# when the line is missing (the command never got to print it).
proc e2k_split_status {output} {
    if {[regexp -indices {\n@@e2k-status (-?[0-9]+)\n?$} $output all code]} {
	set status [string range $output {*}$code]
	set output [string range $output 0 [expr {[lindex $all 0] - 1}]]
	return [list $status $output]
    }
    return [list -1 $output]
}

# A description of exit status CODE as sh reports it.
proc e2k_describe_status {code output} {
    if {$code == 124} {
	return "timeout"
    }
    if {$code > 128 && $code <= 128 + 64} {
	set s "signal [expr {$code - 128}]"
	if {[string match "*exception handling is unimplemented*" $output]} {
	    append s " (qemu stopped on a hardware exception)"
	}
	return $s
    }
    return "exit $code"
}

# Turn a finished run into what DejaGnu's *_load procedures return.
proc e2k_load_result {prog state output limit} {
    global e2k_last_status
    lassign [e2k_split_status $output] code output
    if {$state eq "timeout"} {
	# timeout(1) did not stop it; the board's own limit did.
	set code 124
    } elseif {$state eq "error" || $code < 0} {
	set e2k_last_status "error"
	verbose -log "e2k: could not run $prog ($state): $output"
	return [list "unresolved" $output]
    }
    set e2k_last_status [e2k_describe_status $code $output]
    verbose -log "e2k: $prog: $e2k_last_status" 2
    if {$e2k_last_status eq "timeout"} {
	warning "program timed out."
	verbose -log "e2k: $prog killed after ${limit}s"
    }
    if {$code == 0} {
	return [list "pass" $output]
    }
    return [list "fail" $output]
}

# A program the board needs on the build machine: PATH-relative names are
# looked up, others must exist.
proc e2k_need_program {what prog} {
    if {[file pathtype $prog] eq "relative" && [file tail $prog] eq $prog} {
	if {[auto_execok $prog] eq ""} {
	    perror "e2k board: $what '$prog' is not in PATH"
	    exit 1
	}
    } elseif {![file executable $prog]} {
	perror "e2k board: $what '$prog' does not exist"
	exit 1
    }
}

# ---------------------------------------------------------------------------
# qemu-e2k user mode (protocol "e2kqemu").
#
# The target is local (isremote 0): programs, data files and gcov output
# stay in the build machine's file system, which qemu shares, and GCC's
# testsuite sets the target environment (dg-set-target-env-var) in the
# environment qemu passes on.

# The sysroot for qemu -L: E2K_SYSROOT, else the sysroot of the compiler
# under test (GCC_UNDER_TEST or GXX_UNDER_TEST -print-sysroot, GCC's
# testsuite), else the lcc sysroot. Computed once per board.
proc e2k_qemu_sysroot {dest} {
    if {[board_info $dest exists e2k,sysroot]} {
	return [board_info $dest e2k,sysroot]
    }
    set root ""
    foreach v {GCC_UNDER_TEST GXX_UNDER_TEST} {
	if {$root eq "" && [info exists ::$v] && [set ::$v] ne ""} {
	    if {![catch {exec {*}[set ::$v] -print-sysroot 2>/dev/null} r]} {
		set r [string trim $r]
		if {$r ne "" && [file isdirectory $r]} {
		    set root $r
		}
	    }
	}
    }
    if {$root eq ""} {
	set root [board_info $dest e2k,lcc]/fs
    }
    if {![file isdirectory $root]} {
	perror "e2k board: sysroot $root does not exist (set E2K_SYSROOT)"
    }
    verbose -log "e2k: qemu sysroot $root"
    global board_info
    set board_info($dest,e2k,sysroot) $root
    return $root
}

proc e2kqemu_load {dest prog args} {
    global ld_library_path
    set pargs [lindex $args 0]
    set inp [lindex $args 1]
    if {![file exists $prog]} {
	perror "$prog does not exist in e2kqemu_load."
	return [list "untested" ""]
    }
    set limit [e2k_exec_limit $dest]
    set qemu [list [board_info $dest e2k,qemu] -cpu [board_info $dest e2k,cpu] \
		  -L [e2k_qemu_sysroot $dest]]
    if {[info exists ld_library_path] && [string trim $ld_library_path ":"] ne ""} {
	lappend qemu -E "LD_LIBRARY_PATH=$ld_library_path"
    }
    lappend qemu {*}[board_info $dest e2k,qemu_opts] $prog
    # ulimit -c 0: qemu writes the guest's core file (qemu_PROG_DATE.core)
    # into the current directory when a program dies of a signal.
    set script "exec 2>&1; ulimit -c 0; t=\$1; shift;\
		timeout -k 10 \"\$t\" \"\$@\" $pargs;\
		printf '\\n@@e2k-status %d\\n' \"\$?\""
    set r [e2k_pipe_run [list sh -c $script sh $limit {*}$qemu] $inp \
	       [expr {$limit + 30}]]
    return [e2k_load_result $prog {*}$r $limit]
}

# remote_exec on a local target runs on the build machine without asking
# the protocol, so there is no e2kqemu_exec.

# ---------------------------------------------------------------------------
# An Elbrus machine over SSH (protocol "e2kssh").
#
# Each runtest process works in its own directory, e2k,remote_dir/run-HOST-PID
# (relative paths are relative to the remote home), removed when the run
# closes the target. One master connection serves all ssh calls when
# XDG_RUNTIME_DIR is set (the control socket lives there).
#
# GCC's profile and gcov tests expect the program to write its .gcda file
# at the absolute build-machine path of the object, then fetch, check and
# delete that path with remote_upload and remote_file. Those paths don't
# exist on the remote, so programs run with GCOV_PREFIX_STRIP set to drop
# every directory (libgcov then writes NAME.gcda into the run directory),
# and remote_upload, remote_file exists and remote_file delete look for an
# absolute path that is missing on the remote under its last component in
# the run directory.

# The run directory on the remote, relative to its home.
proc e2k_ssh_dir {dest} {
    return "[board_info $dest e2k,remote_dir]/run-[info hostname]-[pid]"
}

proc e2k_ssh_sockdir {} {
    if {![info exists ::env(XDG_RUNTIME_DIR)] || $::env(XDG_RUNTIME_DIR) eq ""} {
	return ""
    }
    return "$::env(XDG_RUNTIME_DIR)/relcc2-dejagnu-[pid]"
}

# The ssh command line without the host.
proc e2k_ssh_opts {dest} {
    set cmd [list ssh -o BatchMode=yes -o ConnectTimeout=30 \
		 -o ServerAliveInterval=15 -o ServerAliveCountMax=4]
    # relcc2's key only: the board files refuse to load without one.
    if {[board_info $dest e2k,key] eq ""} {
	error "e2k board $dest: no SSH key"
    }
    lappend cmd -i [board_info $dest e2k,key] -o IdentitiesOnly=yes
    if {[board_info $dest e2k,port] ne ""} {
	lappend cmd -p [board_info $dest e2k,port]
    }
    set sock [e2k_ssh_sockdir]
    if {$sock ne ""} {
	file mkdir $sock
	lappend cmd -o ControlMaster=auto -o "ControlPath=$sock/%C" \
	    -o ControlPersist=60
    }
    lappend cmd {*}[board_info $dest e2k,ssh_opts]
    return $cmd
}

# The ssh command line up to and including the host.
proc e2k_ssh_cmd {dest} {
    return [list {*}[e2k_ssh_opts $dest] [board_info $dest e2k,host]]
}

# A remote path for FILE: relative names are in the run directory.
proc e2k_ssh_path {dest file} {
    if {[file pathtype $file] eq "relative"} {
	return "[e2k_ssh_dir $dest]/$file"
    }
    return $file
}

# A shell word for FILE in commands that run in the run directory: an
# absolute FILE that is missing there stands for its last component in
# the run directory (see above).
proc e2k_ssh_word {file} {
    set q [e2k_shq $file]
    if {[file pathtype $file] eq "relative"} {
	return $q
    }
    return "\"\$(test -e $q && echo $q || echo [e2k_shq [file tail $file]])\""
}

# Run the shell command BODY on the remote in the run directory, with stdin
# from the local file INP, for at most LIMIT seconds (plus a margin for
# the connection; BODY must limit itself). Returns {state output}, the
# output still ending in the status line.
proc e2k_ssh_run {dest body inp limit} {
    set d [e2k_shq [e2k_ssh_dir $dest]]
    set script "exec 2>&1; mkdir -p $d && cd $d || exit 1; $body;\
		printf '\\n@@e2k-status %d\\n' \"\$?\""
    global board_info
    set board_info($dest,e2k,dir_used) 1
    return [e2k_pipe_run [list {*}[e2k_ssh_cmd $dest] $script] $inp \
		[expr {$limit + 60}]]
}

proc e2kssh_download {dest file destfile} {
    if {$destfile eq ""} {
	set destfile [file tail $file]
    }
    set path [e2k_ssh_path $dest $destfile]
    set p [e2k_shq $path]
    set t [e2k_shq $path.tmp]
    set script "mkdir -p [e2k_shq [file dirname $path]] && cat > $t &&\
		chmod +x $t && mv -f $t $p"
    global board_info
    set board_info($dest,e2k,dir_used) 1
    if {[catch {exec {*}[e2k_ssh_cmd $dest] $script < $file 2>@1} out]} {
	verbose -log "e2k: copying $file to [board_info $dest e2k,host]:$path failed: $out"
	return ""
    }
    verbose "e2k: copied $file to $path" 2
    return $destfile
}

proc e2kssh_upload {dest srcfile destfile} {
    if {$destfile eq ""} {
	set destfile [file tail $srcfile]
    }
    set d [e2k_shq [e2k_ssh_dir $dest]]
    set script "cd $d && cat [e2k_ssh_word $srcfile]"
    if {[catch {exec {*}[e2k_ssh_cmd $dest] $script < /dev/null \
		    > $destfile 2>/dev/null} out]} {
	verbose -log "e2k: copying [board_info $dest e2k,host]:$srcfile back failed: $out"
	file delete $destfile
	return ""
    }
    return $destfile
}

proc e2kssh_exec {dest program args} {
    set pargs [lindex $args 0]
    set inp [lindex $args 1]
    set outp [lindex $args 2]
    global timeout
    set limit $timeout
    if {![string is integer -strict $limit] || $limit <= 0} {
	set limit 300
    }
    set body "timeout -k 10 $limit sh -c [e2k_shq "$program $pargs"]"
    lassign [e2k_ssh_run $dest $body $inp $limit] state output
    lassign [e2k_split_status $output] code output
    if {$outp ne ""} {
	set f [open $outp w]
	puts -nonewline $f $output
	close $f
    }
    if {$state eq "error" || $code < 0} {
	return [list -1 "e2k: ssh to [board_info $dest e2k,host] failed: $output"]
    }
    return [list $code $output]
}

proc e2kssh_file {dest op args} {
    switch -- $op {
	delete {
	    set files ""
	    foreach f $args {
		append files " [e2k_ssh_word $f]"
	    }
	    set r [e2k_ssh_run $dest "rm -f$files" "" 60]
	    return [expr {[lindex [e2k_split_status [lindex $r 1]] 0] != 0}]
	}
	exists {
	    set r [e2k_ssh_run $dest "test -f [e2k_ssh_word [lindex $args 0]]" "" 60]
	    return [expr {[lindex [e2k_split_status [lindex $r 1]] 0] == 0}]
	}
	default {
	    return [standard_file $dest $op {*}$args]
	}
    }
}

proc e2kssh_load {dest prog args} {
    set pargs [lindex $args 0]
    set inp [lindex $args 1]
    if {![file exists $prog]} {
	perror "$prog does not exist in e2kssh_load."
	return [list "untested" ""]
    }
    set name [file tail $prog]
    if {[e2kssh_download $dest $prog $name] eq ""} {
	global e2k_last_status
	set e2k_last_status "error"
	return [list "unresolved" "e2k: could not copy $prog to [board_info $dest e2k,host]"]
    }
    set limit [e2k_exec_limit $dest]
    set n [e2k_shq ./$name]
    set body "ulimit -c 0; GCOV_PREFIX_STRIP=1000 timeout -k 10 $limit\
	      $n $pargs; st=\$?; rm -f $n; (exit \$st)"
    set r [e2k_ssh_run $dest $body $inp $limit]
    return [e2k_load_result $prog {*}$r $limit]
}

proc e2kssh_close {dest args} {
    global board_info
    if {[info exists board_info($dest,e2k,dir_used)]} {
	unset board_info($dest,e2k,dir_used)
	set dir [e2k_ssh_dir $dest]
	# Remove only this run's directory, then the parent if it is empty
	# (other runs may still use it), but never ~/relcc2 itself.
	if {[regexp {/run-[A-Za-z0-9._-]+$} $dir]} {
	    set script "rm -rf [e2k_shq $dir]; true"
	    set parent [file dirname $dir]
	    if {[file tail $parent] ni {relcc2 . .. ""} && $parent ne "/"} {
		set script "rm -rf [e2k_shq $dir];\
			    rmdir [e2k_shq $parent] 2>/dev/null; true"
	    }
	    catch {exec {*}[e2k_ssh_cmd $dest] $script < /dev/null 2>@1} out
	    verbose "e2k: removed $dir on [board_info $dest e2k,host]" 2
	}
    }
    set sock [e2k_ssh_sockdir]
    if {$sock ne "" && [file isdirectory $sock]} {
	catch {exec {*}[e2k_ssh_opts $dest] -O exit [board_info $dest e2k,host] \
		   < /dev/null 2>@1}
	catch {file delete -force $sock}
    }
    return ""
}
