@echo off
REM 清理Keil编译产物
REM 删除Objects和Listings目录

if exist "Build\Keil\Objects" (
    echo Deleting Objects...
    rmdir /s /q "Build\Keil\Objects"
)

if exist "Build\Keil\Listings" (
    echo Deleting Listings...
    rmdir /s /q "Build\Keil\Listings"
)

echo Clean completed!
