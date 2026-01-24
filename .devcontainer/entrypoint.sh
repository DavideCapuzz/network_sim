
#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------
# ENV & defaults
# -----------------------------------------------
export USER="${USER:-ubuntu}"
export HOME="${HOME:-/home/${USER}}"
export DISPLAY="${DISPLAY:-:1}"

display_num="${DISPLAY#:}"
export VNC_PORT="${VNC_PORT:-$((5900 + display_num))}"
export NOVNC_PORT="${NOVNC_PORT:-${NO_VNC_PORT:-$((6080 + display_num - 1))}}"

PASSWORD="${PASSWORD:-ubuntu}"

# -----------------------------------------------
# Prepare dirs
# -----------------------------------------------
mkdir -p "$HOME/.vnc"
mkdir -p "$HOME/.supervisor"
mkdir -p "$HOME/.fluxbox"

# -----------------------------------------------
# VNC Password
# -----------------------------------------------
echo "$PASSWORD" | vncpasswd -f > "$HOME/.vnc/passwd"
chmod 600 "$HOME/.vnc/passwd"

# -----------------------------------------------
# Print config
# -----------------------------------------------
echo "========================================="
echo "  USER:        $USER"
echo "  HOME:        $HOME"
echo "  DISPLAY:     $DISPLAY"
echo "  VNC Port:    $VNC_PORT"
echo "  noVNC Port:  $NOVNC_PORT"
echo "========================================="


# -----------------------------------------------
# CREATE SUPERVISOR CONFIG (user writable)
# -----------------------------------------------
SUP_CONF="$HOME/.supervisor/supervisord.conf"

cat > "$SUP_CONF" <<EOF
[supervisord]
nodaemon=true
logfile=${HOME}/.supervisor/supervisord.log

[program:vncserver]
command=/usr/bin/Xtigervnc ${DISPLAY} -geometry 1600x900 -depth 24 -rfbport ${VNC_PORT} -SecurityTypes None
environment=HOME="${HOME}",USER="${USER}",DISPLAY="${DISPLAY}"
autorestart=true
priority=10

[program:novnc]
command=/usr/bin/websockify ${NOVNC_PORT} localhost:${VNC_PORT} --web=/usr/share/novnc
environment=HOME="${HOME}",USER="${USER}"
autorestart=true
priority=20

[program:fluxbox]
command=/usr/bin/dbus-launch --exit-with-session /usr/bin/fluxbox
environment=HOME="${HOME}",USER="${USER}",DISPLAY="${DISPLAY}"
autorestart=true
priority=30
EOF


# -----------------------------------------------
# START supervisor through tini (PID 1)
# -----------------------------------------------
exec /usr/bin/tini -- /usr/bin/supervisord -c "$SUP_CONF"
