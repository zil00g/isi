# Security

## Reporting a problem

If you find a security bug in isi, please write to me at
zil00g@protonmail.com before making it public. You can encrypt it to my
key `3945 7779 30D5 1BF1 E3FF  7229 0AB7 30A9 56D3 359D`, the same one
that signs the releases. I'll answer as soon as I can and credit you in
the release notes if you want.

Si lo prefieres, escríbeme en español.

## What encryption protects, and what it doesn't

The format is described in [FORMAT.md](FORMAT.md).

It protects the file on disk: someone who gets a copy can't read it or
change it without the password.

It doesn't protect the text while isi has it open:

- The key is locked in memory and isi disables core dumps, but the open
  text isn't locked, so it can end up in swap. If that matters to you,
  use encrypted swap or none at all.
- Anything you copy with Ctrl+C goes to the system clipboard
  (wl-copy, xclip, xsel or the terminal), in plain text.
- The size of the file is not hidden.
- If someone controls your user account or your machine, they can read
  what you type, password included.

## Checking a release

Each tag and release commit is signed with the key above, and every
release has a `SHA256SUMS` file. The binaries are built in GitHub
Actions from the tagged source and are reproducible.
