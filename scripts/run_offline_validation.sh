#!/bin/bash
set -e

echo "Starting Offline DSP Validation..."

# Ensure we are in the repo root
cd "$(dirname "$0")/.."

# Create output directory for WAV files and reports
mkdir -p tests/output

# Build the project
mkdir -p build
cd build
cmake ..
make

echo "Running CTest automated suites..."
ctest --output-on-failure

echo "Running offline WAV renderer..."
# offline_runner executable generates the WAV files directly into tests/output/
# We run it from the repo root so it can write to tests/output properly
cd ..
./build/tests/offline_runner

echo "Validation complete. Artifacts saved in tests/output/"
ls -lh tests/output/ | head -n 10
echo "... (truncated list)"
