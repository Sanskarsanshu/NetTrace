# NetTrace 3D - Universal Test Runner for Windows PowerShell
$ErrorActionPreference = "Stop"

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   NetTrace 3D - Full Test Suite Runner   " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

$allPassed = $true

# 0. Traceability Check
Write-Host "`n[0/4] Checking Traceability & Test Registration..." -ForegroundColor Yellow
try {
    python scripts/check_traceability.py
    Write-Host "Traceability Check: PASS" -ForegroundColor Green
} catch {
    Write-Host "Traceability Check: FAIL" -ForegroundColor Red
    $allPassed = $false
}

# 0b. Starter Topologies Schema Validation (T-2-34)
Write-Host "`n[0b/4] Validating Example Topologies against Schema..." -ForegroundColor Yellow
try {
    python scripts/validate_examples.py
    Write-Host "Schema Validation: PASS" -ForegroundColor Green
} catch {
    Write-Host "Schema Validation: FAIL" -ForegroundColor Red
    $allPassed = $false
}

# 1. C++ Engine Tests
Write-Host "`n[1/4] Running C++ Engine Tests..." -ForegroundColor Yellow
if (Test-Path "engine/build") {
    try {
        & "C:\Users\sansk\AppData\Roaming\Python\Python314\site-packages\cmake\data\bin\ctest.exe" --test-dir engine/build --output-on-failure
        Write-Host "C++ Engine Tests: PASS" -ForegroundColor Green
    } catch {
        Write-Host "C++ Engine Tests: FAIL" -ForegroundColor Red
        $allPassed = $false
    }
} else {
    Write-Host "C++ Engine build directory not found. Skipping ctest." -ForegroundColor DarkGray
}

# 2. Python Backend Tests
Write-Host "`n[2/3] Running Python Backend Tests..." -ForegroundColor Yellow
if (Test-Path "backend/tests") {
    try {
        python -m pytest backend/tests/ -v
        Write-Host "Backend Tests: PASS" -ForegroundColor Green
    } catch {
        Write-Host "Backend Tests: FAIL" -ForegroundColor Red
        $allPassed = $false
    }
} else {
    Write-Host "backend/tests directory not found. Skipping pytest." -ForegroundColor DarkGray
}

# 3. Frontend Tests
Write-Host "`n[3/3] Running Frontend Tests..." -ForegroundColor Yellow
if (Test-Path "frontend/package.json") {
    try {
        Push-Location frontend
        npm test -- --run
        Pop-Location
        Write-Host "Frontend Tests: PASS" -ForegroundColor Green
    } catch {
        Pop-Location
        Write-Host "Frontend Tests: FAIL" -ForegroundColor Red
        $allPassed = $false
    }
} else {
    Write-Host "frontend/package.json not found. Skipping npm test." -ForegroundColor DarkGray
}

Write-Host "`n==========================================" -ForegroundColor Cyan
if ($allPassed) {
    Write-Host "   All Test Suites Completed Successfully! " -ForegroundColor Green
} else {
    Write-Host "   Some Tests Failed. See Output Above.    " -ForegroundColor Red
    exit 1
}
Write-Host "==========================================" -ForegroundColor Cyan
