@echo off
title KUBE 3D Fitness Plotter
cd /d "%~dp0"
python Fitness3DPlot.py
if %errorlevel% neq 0 pause
