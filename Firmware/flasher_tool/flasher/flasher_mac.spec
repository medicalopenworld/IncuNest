block_cipher = None

from PyInstaller.utils.hooks import collect_data_files

a = Analysis(
    ['main.py'],
    pathex=['.'],
    binaries=[],
    datas=[
        ('logo/IncuNest_logo.png', 'logo'),
    ] + collect_data_files('esptool'),
    hiddenimports=[
        'esptool',
        'esptool.cmds',
        'esptool.loader',
        'esptool.targets',
        'esptool.targets.esp32s3',
        'esptool.targets.esp32',
        'esptool.util',
        'esptool.bin_image',
        'esptool.reset',
        'esptool.config',
        'serial',
        'serial.tools',
        'serial.tools.list_ports',
        'PIL',
        'PIL.Image',
        'PIL.ImageTk',
        'requests',
        'requests.adapters',
        'requests.auth',
        'urllib3',
        'urllib3.util',
        'zeroconf',
        'zeroconf._utils',
        'zeroconf._dns',
        'zeroconf._services',
        'zeroconf._services.browser',
        'zeroconf._handlers',
        'ifaddr',
        'requests_toolbelt',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[],
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name='IncuNest_Flasher',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=False,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=None,
)

app = BUNDLE(
    exe,
    name='IncuNest_Flasher.app',
    icon=None,
    bundle_identifier='com.medicalopenworld.incunestflasher',
)
