#!/bin/bash
set -e

# Default values
HEALTH_CHECK_URL=${HEALTH_CHECK_URL:-"http://localhost:8080/health"}
EXPECTED_STATUS=${EXPECTED_STATUS:-200}
MAX_RETRIES=${MAX_RETRIES:-5}
RETRY_DELAY=${RETRY_DELAY:-10}

echo "Running health check against $HEALTH_CHECK_URL"
echo "Expected status code: $EXPECTED_STATUS"

for i in $(seq 1 $MAX_RETRIES); do
    echo "Attempt $i of $MAX_RETRIES..."
    
    # Perform health check
    STATUS=$(curl -s -o /dev/null -w "%{http_code}" "$HEALTH_CHECK_URL")
    
    if [ "$STATUS" -eq "$EXPECTED_STATUS" ]; then
        echo "Health check passed! (Status: $STATUS)"
        
        # Additional checks
        if [ "$EXPECTED_STATUS" -eq 200 ]; then
            # Check response content
            RESPONSE=$(curl -s "$HEALTH_CHECK_URL")
            if ! echo "$RESPONSE" | jq -e '.status == "healthy"' > /dev/null 2>&1; then
                echo "Warning: Unexpected health check response format"
                echo "Response: $RESPONSE"
                continue
            fi
            
            # Check response time
            RESPONSE_TIME=$(curl -s -w "%{time_total}\n" -o /dev/null "$HEALTH_CHECK_URL")
            if [ "$(echo "$RESPONSE_TIME > 2.0" | bc)" -eq 1 ]; then
                echo "Warning: Response time too high ($RESPONSE_TIME seconds)"
                continue
            fi
        fi
        
        # All checks passed
        echo "All health checks passed successfully"
        exit 0
    else
        echo "Health check failed (Status: $STATUS)"
        if [ $i -lt $MAX_RETRIES ]; then
            echo "Retrying in $RETRY_DELAY seconds..."
            sleep $RETRY_DELAY
        fi
    fi
done

echo "Health check failed after $MAX_RETRIES attempts"
exit 1 