# SPDX-License-Identifier: GPL-3.0-or-later
"""The SSH identity for the Elbrus machines, for relcc2's tools.

The machines must only ever see relcc2's own key: a connection without
it makes ssh offer the user's personal keys, which the machines' owners
see as failed logins.  So the tools never fall back to ssh's defaults:
find_key() returns the key or exits, and options() always adds
IdentitiesOnly=yes.

The key is looked for in this order: the --ssh-key argument, the
RELCC2_SSH_KEY environment variable, this checkout's secrets/id_ed25519,
and the main checkout's (when the tool runs in a git worktree).

    import sshkey
    key = sshkey.find_key(args.ssh_key)       # Path, or sys.exit
    cmd = ["ssh", *sshkey.options(key, port)] + [host, command]
"""

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
NAME = Path("secrets") / "id_ed25519"


def candidates(given=None):
    """Where the key may be, in order."""
    if given:
        yield Path(given)
        return
    env = os.environ.get("RELCC2_SSH_KEY")
    if env:
        yield Path(env)
        return
    yield ROOT / NAME
    try:
        common = subprocess.run(
            ["git", "-C", str(ROOT), "rev-parse", "--path-format=absolute",
             "--git-common-dir"], capture_output=True, text=True,
            check=True).stdout.strip()
        yield Path(common).parent / NAME
    except (OSError, subprocess.CalledProcessError):
        pass


def find_key(given=None):
    """The identity file, or exit without connecting."""
    tried = []
    for p in candidates(given):
        if p.is_file():
            return p.resolve()
        tried.append(str(p))
    sys.exit("no SSH key for the Elbrus machines (tried "
             + ", ".join(tried) + "): pass --ssh-key or set "
             "RELCC2_SSH_KEY; refusing to connect with ssh's own keys")


def options(key, port=None, host_key_alias=None):
    """ssh/scp options that use KEY and nothing else.  HOST_KEY_ALIAS is
    the name the machine's host key is known by, for a host given by
    address (when its .lan name doesn't resolve)."""
    o = ["-i", str(key), "-o", "IdentitiesOnly=yes", "-o", "BatchMode=yes"]
    if port:
        o += ["-o", f"Port={port}"]
    if host_key_alias:
        o += ["-o", f"HostKeyAlias={host_key_alias}"]
    return o
