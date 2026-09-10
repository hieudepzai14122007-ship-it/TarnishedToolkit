"""Create a source + binary distribution without research clones or local state."""
from pathlib import Path
from zipfile import ZipFile, ZIP_DEFLATED
import hashlib

root = Path(__file__).resolve().parent
output = root / 'dist'
output.mkdir(exist_ok=True)
archive = output / 'TarnishedToolkit-0.2.6-flying.zip'
folders = ('src', 'tests', 'vendor', 'docs', 'licenses', 'provenance')
files = ['README.md', 'LICENSE.txt', 'THIRD_PARTY_NOTICES.md', 'CMakeLists.txt',
         'Build.ps1', 'Install.ps1', 'Update.ps1', 'Uninstall.ps1', 'Package.py',
         'LaunchLayout.ps1', 'SeparateLaunches.ps1', 'LaunchOffline.ps1',
         'CreateLaunchShortcuts.ps1',
         'GenerateCatalog.py', 'GenerateProfile.py',
         'build/TarnishedToolkit.dll', 'build/core_tests.exe', 'build/storage_tests.exe', 'build/attribute_tests.exe', 'build/horse_jump_tests.exe', 'build/renderer_test.exe']
with ZipFile(archive, 'w', ZIP_DEFLATED, compresslevel=9) as z:
    for folder in folders:
        for p in sorted((root / folder).rglob('*')):
            if p.is_file() and '.git' not in p.relative_to(root).parts:
                z.write(p, Path('TarnishedToolkit') / p.relative_to(root))
    for name in files:
        p = root / name
        z.write(p, Path('TarnishedToolkit') / name)
with ZipFile(archive) as z:
    bad = z.testzip()
    if bad:
        raise RuntimeError(f'Archive validation failed: {bad}')
sha = hashlib.sha256(archive.read_bytes()).hexdigest()
(output / 'SHA256SUMS.txt').write_text(f'{sha}  {archive.name}\n', encoding='utf-8')
print(f'{archive}\n{archive.stat().st_size:,} bytes\nSHA256 {sha}')
