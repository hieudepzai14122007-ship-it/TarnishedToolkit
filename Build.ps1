param([string]$Toolchain = 'C:\msys64\ucrt64\bin')
$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$out = Join-Path $root 'build'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$cc = Join-Path $Toolchain 'gcc.exe'
$cxx = Join-Path $Toolchain 'g++.exe'
if (!(Test-Path -LiteralPath $cxx)) { throw 'Install the MSYS2 UCRT64 GCC toolchain or pass -Toolchain.' }
$cFiles = @('buffer.c','hook.c','trampoline.c','hde\hde64.c')
$objects = @()
foreach ($file in $cFiles) {
    $source = Join-Path $root ('vendor\minhook\src\' + $file)
    $obj = Join-Path $out (($file -replace '[\\.]','_') + '.o')
    & $cc '-O2' '-c' $source '-o' $obj
    if ($LASTEXITCODE) { throw "Compilation failed: $file" }
    $objects += $obj
}
$sources = @('src\dllmain.cpp','src\runtime.cpp','src\ui.cpp','src\overlay.cpp','src\input.cpp','src\horse_jump.cpp','src\catalog.cpp','src\storage.cpp','vendor\imgui\imgui.cpp','vendor\imgui\imgui_draw.cpp','vendor\imgui\imgui_tables.cpp','vendor\imgui\imgui_widgets.cpp','vendor\imgui\backends\imgui_impl_win32.cpp','vendor\imgui\backends\imgui_impl_dx12.cpp') | ForEach-Object { Join-Path $root $_ }
$includes = @('src','vendor\imgui','vendor\imgui\backends','vendor\minhook\include') | ForEach-Object { '-I' + (Join-Path $root $_) }
& $cxx '-std=c++20' '-O2' '-shared' '-static' '-static-libgcc' '-static-libstdc++' '-DUNICODE' '-D_UNICODE' '-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD' @includes @sources @objects '-ld3d12' '-ldxgi' '-ld3dcompiler' '-ldwmapi' '-lgdi32' '-lbcrypt' '-ldinput8' '-ldxguid' '-o' (Join-Path $out 'TarnishedToolkit.dll')
if ($LASTEXITCODE) { throw 'DLL build failed.' }
& $cxx '-std=c++20' '-O2' '-static' '-static-libgcc' '-static-libstdc++' ('-I' + (Join-Path $root 'src')) (Join-Path $root 'tests\core_tests.cpp') (Join-Path $root 'src\catalog.cpp') '-o' (Join-Path $out 'core_tests.exe')
if ($LASTEXITCODE) { throw 'Test build failed.' }
& (Join-Path $out 'core_tests.exe')
if ($LASTEXITCODE) { throw 'Tests failed.' }
& $cxx '-std=c++20' '-O2' '-static' '-static-libgcc' '-static-libstdc++' ('-I' + (Join-Path $root 'src')) (Join-Path $root 'tests\storage_tests.cpp') (Join-Path $root 'src\storage.cpp') (Join-Path $root 'src\catalog.cpp') '-o' (Join-Path $out 'storage_tests.exe')
if ($LASTEXITCODE) { throw 'Storage test build failed.' }
& (Join-Path $out 'storage_tests.exe')
if ($LASTEXITCODE) { throw 'Storage tests failed.' }
& $cxx '-std=c++20' '-O2' '-static' '-static-libgcc' '-static-libstdc++' ('-I' + (Join-Path $root 'src')) (Join-Path $root 'tests\attribute_tests.cpp') '-o' (Join-Path $out 'attribute_tests.exe')
if ($LASTEXITCODE) { throw 'Attribute test build failed.' }
& (Join-Path $out 'attribute_tests.exe')
if ($LASTEXITCODE) { throw 'Attribute tests failed.' }
& $cxx '-std=c++20' '-O2' '-static' '-static-libgcc' '-static-libstdc++' '-DTT_HORSE_TESTS' @includes (Join-Path $root 'tests\horse_jump_tests.cpp') (Join-Path $root 'src\horse_jump.cpp') @objects '-o' (Join-Path $out 'horse_jump_tests.exe')
if ($LASTEXITCODE) { throw 'Horse jump test build failed.' }
& (Join-Path $out 'horse_jump_tests.exe')
if ($LASTEXITCODE) { throw 'Horse jump tests failed.' }
& $cxx '-std=c++20' '-O2' '-static' '-static-libgcc' '-static-libstdc++' '-mwindows' (Join-Path $root 'tests\renderer_host.cpp') '-ld3d12' '-ldxgi' '-o' (Join-Path $out 'renderer_test.exe')
if ($LASTEXITCODE) { throw 'Renderer test build failed.' }
& (Join-Path $root 'tests\launch_layout_tests.ps1')
Get-Item -LiteralPath (Join-Path $out 'TarnishedToolkit.dll') | Select-Object FullName,Length
