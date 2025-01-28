#!/bin/bash
set -e

# Configuration
APP_NAME="myapp"
DEPLOY_ROOT="/opt/releasy/staging"
VERSION="$1"

if [ -z "$VERSION" ]; then
    echo "Error: Version parameter is required"
    exit 1
fi

echo "Deploying version $VERSION to staging environment..."

# Create deployment directory
DEPLOY_DIR="$DEPLOY_ROOT/$APP_NAME-$VERSION"
mkdir -p "$DEPLOY_DIR"

# Download and extract application
echo "Downloading application package..."
curl -sSL "https://artifacts.example.com/$APP_NAME-$VERSION.tar.gz" \
    -o "/tmp/$APP_NAME-$VERSION.tar.gz"

echo "Extracting package to $DEPLOY_DIR..."
tar -xzf "/tmp/$APP_NAME-$VERSION.tar.gz" -C "$DEPLOY_DIR"
rm -f "/tmp/$APP_NAME-$VERSION.tar.gz"

# Update configuration
echo "Updating application configuration..."
envsubst < "$DEPLOY_DIR/config/app.conf.template" > "$DEPLOY_DIR/config/app.conf"

# Update symlink
CURRENT_LINK="$DEPLOY_ROOT/current"
if [ -L "$CURRENT_LINK" ]; then
    OLD_TARGET=$(readlink "$CURRENT_LINK")
    mv "$CURRENT_LINK" "$CURRENT_LINK.previous"
fi

ln -sf "$DEPLOY_DIR" "$CURRENT_LINK"

# Restart application
echo "Restarting application..."
systemctl restart "$APP_NAME-staging"

# Wait for application to start
echo "Waiting for application to start..."
for i in {1..30}; do
    if curl -s "http://localhost:$APP_PORT/health" | grep -q "ok"; then
        echo "Application started successfully"
        exit 0
    fi
    sleep 1
done

echo "Error: Application failed to start within timeout"
if [ -L "$CURRENT_LINK.previous" ]; then
    echo "Rolling back to previous version..."
    mv "$CURRENT_LINK.previous" "$CURRENT_LINK"
    systemctl restart "$APP_NAME-staging"
fi

exit 1 