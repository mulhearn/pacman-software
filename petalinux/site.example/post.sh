#!/bin/sh
# Site edits to the image's own files:  $1 image root, $2 site directory
if [ -f "$2/root.hash" ]; then
    sed -i "s|^root:[^:]*:|root:$(cat "$2/root.hash"):|" "$1/etc/shadow"
fi
