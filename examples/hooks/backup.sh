#!/bin/bash
set -e

# Configuration
BACKUP_DIR=${BACKUP_DIR:-"/var/backups/releasy"}
APP_NAME=${APP_NAME:-"myapp"}
DEPLOY_ENV=${DEPLOY_ENV:-"unknown"}
RETENTION_DAYS=${RETENTION_DAYS:-7}
COMPRESS_LEVEL=${COMPRESS_LEVEL:-9}

# Create timestamp
TIMESTAMP=$(date +%Y%m%d_%H%M%S)
BACKUP_PATH="$BACKUP_DIR/$APP_NAME-$DEPLOY_ENV-$TIMESTAMP"

echo "Starting backup process..."
echo "Backup location: $BACKUP_PATH"

# Ensure backup directory exists
mkdir -p "$BACKUP_DIR"

# Get current deployment path
CURRENT_LINK="/opt/releasy/$DEPLOY_ENV/current"
if [ ! -L "$CURRENT_LINK" ]; then
    echo "Error: Current deployment link not found"
    exit 1
fi

CURRENT_PATH=$(readlink -f "$CURRENT_LINK")
if [ ! -d "$CURRENT_PATH" ]; then
    echo "Error: Current deployment directory not found"
    exit 1
fi

# Create backup
echo "Creating backup of $CURRENT_PATH..."

# 1. Create directory structure
mkdir -p "$BACKUP_PATH"

# 2. Copy files
echo "Copying files..."
cp -a "$CURRENT_PATH/." "$BACKUP_PATH/"

# 3. Export database if exists
if [ -f "$CURRENT_PATH/config/database.yml" ]; then
    echo "Database configuration found, exporting database..."
    
    # Parse database configuration
    DB_NAME=$(grep "database:" "$CURRENT_PATH/config/database.yml" | awk '{print $2}')
    DB_USER=$(grep "username:" "$CURRENT_PATH/config/database.yml" | awk '{print $2}')
    DB_PASS=$(grep "password:" "$CURRENT_PATH/config/database.yml" | awk '{print $2}')
    
    if [ -n "$DB_NAME" ]; then
        echo "Backing up database $DB_NAME..."
        PGPASSWORD="$DB_PASS" pg_dump -U "$DB_USER" "$DB_NAME" > "$BACKUP_PATH/database.sql"
    fi
fi

# 4. Compress backup
echo "Compressing backup..."
cd "$BACKUP_DIR"
tar -czf "$BACKUP_PATH.tar.gz" -C "$BACKUP_PATH" . --remove-files

# 5. Calculate checksums
echo "Calculating checksums..."
sha256sum "$BACKUP_PATH.tar.gz" > "$BACKUP_PATH.tar.gz.sha256"
md5sum "$BACKUP_PATH.tar.gz" > "$BACKUP_PATH.tar.gz.md5"

# 6. Clean up old backups
echo "Cleaning up old backups..."
find "$BACKUP_DIR" -name "$APP_NAME-$DEPLOY_ENV-*.tar.gz" -mtime +$RETENTION_DAYS -delete
find "$BACKUP_DIR" -name "$APP_NAME-$DEPLOY_ENV-*.sha256" -mtime +$RETENTION_DAYS -delete
find "$BACKUP_DIR" -name "$APP_NAME-$DEPLOY_ENV-*.md5" -mtime +$RETENTION_DAYS -delete

# 7. Create backup report
echo "Creating backup report..."
cat > "$BACKUP_PATH.report.txt" << EOF
Backup Report
============

Application: $APP_NAME
Environment: $DEPLOY_ENV
Timestamp: $TIMESTAMP
Source Path: $CURRENT_PATH
Backup Path: $BACKUP_PATH.tar.gz

File Information:
----------------
Size: $(du -h "$BACKUP_PATH.tar.gz" | cut -f1)
SHA256: $(cat "$BACKUP_PATH.tar.gz.sha256")
MD5: $(cat "$BACKUP_PATH.tar.gz.md5")

Configuration:
-------------
Retention Days: $RETENTION_DAYS
Compression Level: $COMPRESS_LEVEL

Database Backup: $([ -f "$BACKUP_PATH/database.sql" ] && echo "Yes" || echo "No")
EOF

echo "Backup completed successfully"
echo "Backup file: $BACKUP_PATH.tar.gz"
echo "Report file: $BACKUP_PATH.report.txt"
exit 0 