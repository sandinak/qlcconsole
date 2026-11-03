# Sourced by check-all.sh, dev-app.sh and package-local.sh (not run): picks
# the ccache directory for this Mac.
#
# ender is a laptop with a 1.8 TB work drive that is mounted only while it is
# docked. A cache of ~100 GB lives there (/Volumes/Ext/ccache) while the drive
# is mounted; undocked, ccache uses its normal internal cache (raised to
# 20 GB, ~/Library/Preferences/ccache/ccache.conf), so an undocked build is
# colder, never broken. ccache.conf cannot be conditional, hence this.
#
# The switch is the directory: it is used only when /Volumes/Ext/ccache exists
# and a file can be written in it (a mounted drive that went read-only, or an
# unmounted /Volumes/Ext, counts as absent). Other Macs have no such directory,
# so they keep their own caches. An exported CCACHE_DIR always wins.
# Its size is that directory's ccache.conf (max_size = 100G, written by
# `ccache -d /Volumes/Ext/ccache -M 100G` or here on first use).
QLC_CCACHE_EXT="${QLC_CCACHE_EXT:-/Volumes/Ext/ccache}"
if [ -z "${CCACHE_DIR:-}" ] && [ -d "$QLC_CCACHE_EXT" ] && [ -w "$QLC_CCACHE_EXT" ] \
   && ( : > "$QLC_CCACHE_EXT/.qlc-write-test" ) 2>/dev/null; then
    rm -f "$QLC_CCACHE_EXT/.qlc-write-test"
    export CCACHE_DIR="$QLC_CCACHE_EXT"
    [ -f "$CCACHE_DIR/ccache.conf" ] || echo "max_size = 100G" > "$CCACHE_DIR/ccache.conf" 2>/dev/null || true
fi
