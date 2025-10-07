#!/bin/bash
# Start Regulens server for E2E testing

echo "Starting Regulens with Docker Compose..."
cd ../.. || exit 1

# Start services
docker-compose up -d

# Wait for PostgreSQL to be ready
echo "Waiting for PostgreSQL..."
until docker-compose exec -T postgres pg_isready -U regulens_user -d regulens_compliance > /dev/null 2>&1; do
  sleep 1
done
echo "✓ PostgreSQL is ready"

# Wait for Redis to be ready
echo "Waiting for Redis..."
until docker-compose exec -T redis redis-cli ping > /dev/null 2>&1; do
  sleep 1
done
echo "✓ Redis is ready"

# Wait for Regulens app to be ready
echo "Waiting for Regulens application on http://localhost:8080..."
timeout=60
counter=0
until curl -f http://localhost:8080/api/health > /dev/null 2>&1; do
  counter=$((counter + 1))
  if [ $counter -gt $timeout ]; then
    echo "❌ Timeout waiting for Regulens application"
    echo "Check logs with: docker-compose logs regulens"
    exit 1
  fi
  sleep 1
done

echo "✓ Regulens is ready at http://localhost:8080"
echo ""
echo "You can now run tests with:"
echo "  npm test"
echo ""
echo "To stop the server:"
echo "  docker-compose down"
