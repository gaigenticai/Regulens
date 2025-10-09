#!/bin/bash

echo "Stopping containers..."
docker-compose down

echo "Rebuilding frontend with no cache..."
docker-compose build --no-cache frontend

echo "Starting services..."
docker-compose up -d

echo "Waiting for services to start..."
sleep 10

echo "Checking frontend logs..."
docker logs regulens-frontend

echo "Frontend should be accessible at http://localhost:3000"