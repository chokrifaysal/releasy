#!/bin/bash

echo "Deploying to staging environment..."
echo "Environment variables:"
env | grep "^DEPLOY_"
echo "Working directory: $(pwd)"
echo "Deployment successful"
exit 0 