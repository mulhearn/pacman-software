# Site overlay:  the untracked site directory applied to the root filesystem;  without it the images build as
# released.

PACMAN_SITE ?= "${@os.path.abspath(d.getVar('TOPDIR') + '/../../site')}"

# Rebuild the root filesystem when a site file changes
def pacman_site_files(d):
    import os
    site = d.getVar('PACMAN_SITE')
    files = []
    for top, dirs, names in os.walk(site):
        for n in sorted(names):
            files.append(os.path.join(top, n) + ':True')
    return ' '.join(files)

do_rootfs[file-checksums] += "${@pacman_site_files(d)}"

pacman_site_overlay() {
    site="${PACMAN_SITE}"
    if [ ! -d "$site" ]; then
        bbnote "PACMAN site overlay:  none ($site)"
        return 0
    fi
    if [ -d "$site/rootfs" ]; then
        ( cd "$site/rootfs" && find . -type f -o -type l | sort | while read f; do
            bbnote "PACMAN site overlay:  $f $(sha256sum "$f" | cut -c1-8)"
          done )
        # file modes kept, owner root;  directories already in the image keep their own modes
        tar -C "$site/rootfs" -cf - . | tar -C "${IMAGE_ROOTFS}" -xf - --no-same-owner --no-overwrite-dir
    fi
    if [ -f "$site/post.sh" ]; then
        bbnote "PACMAN site overlay:  post.sh $(sha256sum "$site/post.sh" | cut -c1-8)"
        sh "$site/post.sh" "${IMAGE_ROOTFS}" "$site" || bbfatal "PACMAN site overlay:  post.sh failed"
    fi
}

ROOTFS_POSTPROCESS_COMMAND += "pacman_site_overlay; "
