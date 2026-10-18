"""Compile and execute the actual C++ stamina model without Unreal Engine."""
from pathlib import Path
import os
import shutil
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parents[1]
    output = root / "Tests" / "bin"
    output.mkdir(parents=True, exist_ok=True)
    include = root / "Plugins/MechanicsHomework/Source/MechanicsHomework/Public"
    source = root / "Tests/StaminaModelTests.cpp"
    env = os.environ.copy()
    compiler = shutil.which("cl") if os.name == "nt" else None
    if os.name == "nt" and not compiler:
        vswhere = Path(env.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        if vswhere.exists():
            install = subprocess.check_output([
                str(vswhere), "-latest", "-products", "*", "-requires",
                "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"
            ], text=True).strip()
            if install:
                developer = Path(install) / "Common7/Tools/VsDevCmd.bat"
                # Capture the environment internally; never print its contents.
                command = f'call "{developer}" -arch=x64 -host_arch=x64 >nul && set'
                completed = subprocess.run(
                    command, shell=True,
                    check=True, capture_output=True, text=True,
                )
                for line in completed.stdout.splitlines():
                    key, sep, value = line.partition("=")
                    if sep and key:
                        env[key] = value
                search_path = next((value for key, value in env.items() if key.upper() == "PATH" and "MSVC" in value), env.get("PATH", ""))
                compiler = shutil.which("cl", path=search_path)
    executable = output / ("stamina-tests.exe" if os.name == "nt" else "stamina-tests")
    if compiler:
        command = [compiler, "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX",
                   f"/I{include}", str(source), f"/Fe{executable}", f"/Fo{output / 'stamina-tests.obj'}"]
    else:
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            raise SystemExit("Install a C++ compiler: MSVC, g++ or clang++.")
        command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(include), str(source), "-o", str(executable)]
    subprocess.run(command, cwd=output, env=env, check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
