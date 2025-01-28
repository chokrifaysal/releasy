#!/bin/bash
set -e

# Configuration
MAINTENANCE_FILE="/var/www/html/maintenance.html"
NGINX_CONF="/etc/nginx/sites-enabled/myapp"
ACTION="$1"

if [ -z "$ACTION" ]; then
    echo "Error: Action parameter (enable/disable) is required"
    exit 1
fi

case "$ACTION" in
    "enable")
        echo "Enabling maintenance mode..."
        
        # Create maintenance page if it doesn't exist
        if [ ! -f "$MAINTENANCE_FILE" ]; then
            cat > "$MAINTENANCE_FILE" << EOF
<!DOCTYPE html>
<html>
<head>
    <title>Maintenance Mode</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            text-align: center;
            padding-top: 100px;
            background-color: #f5f5f5;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            padding: 20px;
            background-color: white;
            border-radius: 5px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        h1 { color: #333; }
        p { color: #666; }
    </style>
</head>
<body>
    <div class="container">
        <h1>System Maintenance</h1>
        <p>We're currently performing system updates. Please check back in a few minutes.</p>
        <p>We apologize for any inconvenience.</p>
    </div>
</body>
</html>
EOF
        fi

        # Update Nginx configuration
        if ! grep -q "maintenance_mode" "$NGINX_CONF"; then
            # Backup original configuration
            cp "$NGINX_CONF" "$NGINX_CONF.bak"
            
            # Insert maintenance mode configuration
            sed -i '/location \/ {/i \
    # Maintenance mode\
    if (-f $document_root/maintenance.html) {\
        return 503;\
    }\
\
    error_page 503 @maintenance;\
    location @maintenance {\
        rewrite ^(.*)$ /maintenance.html break;\
    }\
' "$NGINX_CONF"
        fi

        # Reload Nginx
        echo "Reloading Nginx configuration..."
        systemctl reload nginx
        
        echo "Maintenance mode enabled"
        ;;
        
    "disable")
        echo "Disabling maintenance mode..."
        
        # Remove maintenance page
        if [ -f "$MAINTENANCE_FILE" ]; then
            rm -f "$MAINTENANCE_FILE"
        fi
        
        # Restore original Nginx configuration if backup exists
        if [ -f "$NGINX_CONF.bak" ]; then
            mv "$NGINX_CONF.bak" "$NGINX_CONF"
            
            # Reload Nginx
            echo "Reloading Nginx configuration..."
            systemctl reload nginx
        fi
        
        echo "Maintenance mode disabled"
        ;;
        
    *)
        echo "Error: Invalid action. Use 'enable' or 'disable'"
        exit 1
        ;;
esac

exit 0 