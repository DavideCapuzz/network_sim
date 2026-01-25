# =====================================================================
# 2. IMPROVED entrypoint.sh (Based on your existing)
# =====================================================================
# FILE: entrypoint.sh
---
#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------
# ENV & defaults (keeping your style)
# -----------------------------------------------
export USER="${USER:-ubuntu}"
export HOME="${HOME:-/home/${USER}}"
export DISPLAY="${DISPLAY:-:1}"

display_num="${DISPLAY#:}"
export VNC_PORT="${VNC_PORT:-$((5900 + display_num))}"
export NOVNC_PORT="${NOVNC_PORT:-${NO_VNC_PORT:-$((6080 + display_num - 1))}}"

PASSWORD="${PASSWORD:-ubuntu}"

# -----------------------------------------------
# Network performance tuning (NEW)
# -----------------------------------------------
echo "Applying network performance tuning..."
sudo sysctl -w net.core.rmem_max=134217728 2>/dev/null || true
sudo sysctl -w net.core.wmem_max=134217728 2>/dev/null || true
sudo sysctl -w net.ipv4.udp_mem="65536 131072 262144" 2>/dev/null || true
sudo sysctl -w net.core.netdev_max_backlog=5000 2>/dev/null || true

# -----------------------------------------------
# Prepare dirs (keeping your style)
# -----------------------------------------------
mkdir -p "$HOME/.vnc"
mkdir -p "$HOME/.supervisor"
mkdir -p "$HOME/.fluxbox"
mkdir -p "$HOME/results"

# -----------------------------------------------
# VNC Password (keeping your style)
# -----------------------------------------------
echo "$PASSWORD" | vncpasswd -f > "$HOME/.vnc/passwd"
chmod 600 "$HOME/.vnc/passwd"

# -----------------------------------------------
# Print config (enhanced)
# -----------------------------------------------
echo "========================================="
echo "  OMNeT++ Multi-Robot Simulation"
echo "========================================="
echo "  USER:        $USER"
echo "  HOME:        $HOME"
echo "  DISPLAY:     $DISPLAY"
echo "  VNC Port:    $VNC_PORT"
echo "  noVNC Port:  $NOVNC_PORT"
echo ""
echo "  Access GUI:  http://localhost:${NOVNC_PORT}"
echo "========================================="

# -----------------------------------------------
# CREATE SUPERVISOR CONFIG (keeping your approach)
# -----------------------------------------------
SUP_CONF="$HOME/.supervisor/supervisord.conf"

cat > "$SUP_CONF" <<EOF
[supervisord]
nodaemon=true
logfile=${HOME}/.supervisor/supervisord.log
loglevel=info

[program:vncserver]
command=/usr/bin/Xtigervnc ${DISPLAY} -geometry ${VNC_RESOLUTION} -depth 24 -rfbport ${VNC_PORT} -SecurityTypes None
environment=HOME="${HOME}",USER="${USER}",DISPLAY="${DISPLAY}"
autorestart=true
priority=10
stdout_logfile=${HOME}/.supervisor/vncserver.log
stderr_logfile=${HOME}/.supervisor/vncserver.err

[program:novnc]
command=/usr/bin/websockify ${NOVNC_PORT} localhost:${VNC_PORT} --web=/usr/share/novnc
environment=HOME="${HOME}",USER="${USER}"
autorestart=true
priority=20
stdout_logfile=${HOME}/.supervisor/novnc.log
stderr_logfile=${HOME}/.supervisor/novnc.err

[program:fluxbox]
command=/usr/bin/dbus-launch --exit-with-session /usr/bin/fluxbox
environment=HOME="${HOME}",USER="${USER}",DISPLAY="${DISPLAY}"
autorestart=true
priority=30
stdout_logfile=${HOME}/.supervisor/fluxbox.log
stderr_logfile=${HOME}/.supervisor/fluxbox.err
EOF

# -----------------------------------------------
# START supervisor through tini (keeping your approach)
# -----------------------------------------------
exec /usr/bin/supervisord -c "$SUP_CONF"