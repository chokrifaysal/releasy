#!/bin/bash
set -e

# Configuration
SLACK_WEBHOOK_URL=${SLACK_WEBHOOK_URL:-""}
EMAIL_TO=${EMAIL_TO:-"ops@example.com"}
EVENT="$1"

if [ -z "$EVENT" ]; then
    echo "Error: Event parameter is required"
    exit 1
fi

# Get deployment information
APP_VERSION=${APP_VERSION:-"unknown"}
DEPLOY_ENV=${DEPLOY_ENV:-"unknown"}
DEPLOY_USER=${DEPLOY_USER:-$(whoami)}
DEPLOY_TIME=$(date '+%Y-%m-%d %H:%M:%S')

# Prepare message based on event
case "$EVENT" in
    "start")
        SUBJECT="Deployment Started: $APP_VERSION to $DEPLOY_ENV"
        MESSAGE="🚀 Deployment started
Environment: $DEPLOY_ENV
Version: $APP_VERSION
Started by: $DEPLOY_USER
Time: $DEPLOY_TIME"
        ;;
        
    "complete")
        SUBJECT="Deployment Complete: $APP_VERSION to $DEPLOY_ENV"
        MESSAGE="✅ Deployment completed successfully
Environment: $DEPLOY_ENV
Version: $APP_VERSION
Deployed by: $DEPLOY_USER
Time: $DEPLOY_TIME"
        ;;
        
    "failed")
        SUBJECT="Deployment Failed: $APP_VERSION to $DEPLOY_ENV"
        MESSAGE="❌ Deployment failed
Environment: $DEPLOY_ENV
Version: $APP_VERSION
Attempted by: $DEPLOY_USER
Time: $DEPLOY_TIME
Please check logs for details."
        ;;
        
    "rollback")
        SUBJECT="Deployment Rolled Back: $APP_VERSION in $DEPLOY_ENV"
        MESSAGE="⚠️ Deployment rolled back
Environment: $DEPLOY_ENV
Version: $APP_VERSION
Rolled back by: $DEPLOY_USER
Time: $DEPLOY_TIME"
        ;;
        
    *)
        echo "Error: Invalid event. Use 'start', 'complete', 'failed', or 'rollback'"
        exit 1
        ;;
esac

# Send notifications

# 1. Slack notification
if [ -n "$SLACK_WEBHOOK_URL" ]; then
    echo "Sending Slack notification..."
    curl -s -X POST -H 'Content-type: application/json' \
        --data "{\"text\":\"$MESSAGE\"}" \
        "$SLACK_WEBHOOK_URL"
fi

# 2. Email notification
if [ -n "$EMAIL_TO" ]; then
    echo "Sending email notification..."
    echo "$MESSAGE" | mail -s "$SUBJECT" "$EMAIL_TO"
fi

# 3. Log to system journal
echo "Logging to system journal..."
logger -t "releasy-deploy" "$SUBJECT - $MESSAGE"

echo "Notifications sent successfully"
exit 0 