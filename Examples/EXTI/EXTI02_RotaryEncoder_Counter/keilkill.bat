@echo off
echo Cleaning build artifacts...

if exist "Build" (
    rmdir /s /q "Build"
    echo Build directory removed.
)

if exist "Listings" (
    rmdir /s /q "Listings"
    echo Listings directory removed.
)

if exist "Objects" (
    rmdir /s /q "Objects"
    echo Objects directory removed.
)

echo Clean complete.


