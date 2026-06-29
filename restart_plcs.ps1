# Script to restart CODESYS PLCs and reset the 2-hour demo timer
Write-Host "Stopping and restarting CODESYS PLC containers to reset the 2-hour demo timer..." -ForegroundColor Cyan

# Restart both PLC containers using docker compose
docker compose restart novatech-warsaw-assembly-line1-plc1 novatech-warsaw-assembly-line1-plc2

Write-Host "PLCs restarted successfully! Demo timer reset to 2 hours. Programs started from scratch." -ForegroundColor Green
