#!/bin/bash
# ================================================================
#  GitHub Push – ESP32-ALCAM
#  Nach jeder Codeänderung ausführen: bash git_push.sh
# ================================================================

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$REPO_DIR"

# Token aus secrets.local lesen (wird NICHT zu GitHub hochgeladen)
if [ -f "$REPO_DIR/secrets.local" ]; then
  source "$REPO_DIR/secrets.local"
  GH_USER="hondamz"
  REPO_NAME="ESP32-ALCAM"
  git remote set-url origin "https://${GH_USER}:${GH_TOKEN}@github.com/${GH_USER}/${REPO_NAME}.git" 2>/dev/null || true
fi

# Versions-Tag aus dem Sketch lesen (falls vorhanden)
VERSION=$(grep -r '#define APP_VERSION' . --include="*.ino" \
          | sed 's/.*"\(.*\)".*/\1/' 2>/dev/null | head -1 || echo "dev")

echo ""
echo "=== GitHub Push – ESP32-ALCAM v${VERSION} ==="

# Status prüfen
CHANGED=$(git status --short | wc -l | tr -d ' ')
if [ "$CHANGED" = "0" ]; then
  echo "Keine Änderungen – nichts zu tun."
  exit 0
fi

echo "Geänderte Dateien:"
git status --short

# Commit mit Zeitstempel
TIMESTAMP=$(date "+%Y-%m-%d %H:%M")
git add -A
git commit -m "v${VERSION} – Update ${TIMESTAMP}

$(git diff --cached --name-only --diff-filter=M 2>/dev/null | sed 's/^/- Geaendert: /' || true)
$(git diff --cached --name-only --diff-filter=A 2>/dev/null | sed 's/^/- Neu: /' || true)"

git push origin main

echo ""
echo "✓ Gepusht: https://github.com/hondamz/ESP32-ALCAM"
echo ""
