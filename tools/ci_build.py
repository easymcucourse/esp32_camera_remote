"""Build either firmware with fresh, portable CI defaults in an exported IDF shell."""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('board', choices=('lcd', 'atom'))
    parser.add_argument('flavour', choices=('debug', 'release'))
    parser.add_argument('--profile', choices=('default', 'stable'), default='default',
                        help='LCD stable profile uses 80 MHz Flash and PSRAM')
    parser.add_argument('--build-tag', help='Separate measurement build directory (letters, digits, hyphens)')
    args = parser.parse_args()
    if args.build_tag and not re.fullmatch(r'[a-z][a-z0-9-]{0,47}', args.build_tag):
        parser.error('build-tag must start with a lowercase letter and contain at most 48 letters/digits/hyphens')
    root = Path(__file__).resolve().parents[1]
    project = root if args.board == 'lcd' else root / 'm5_atom_matrix'
    if args.profile == 'stable' and args.board != 'lcd':
        parser.error('stable profile is for the LCD ESP32-S3 only')
    suffix = '-stable' if args.profile == 'stable' else ''
    if args.build_tag:
        suffix += '-' + args.build_tag
    build = root / 'build' / f'ci-{args.board}-{args.flavour}{suffix}'
    idf = os.environ.get('IDF_PATH')
    if not idf or not (Path(idf) / 'tools/idf.py').is_file():
        parser.error('export ESP-IDF first (IDF_PATH must point to its checkout)')
    target = 'esp32s3' if args.board == 'lcd' else 'esp32'
    default_files = [project / 'sdkconfig.defaults', root / 'tools/ci' / f'{args.flavour}.defaults']
    if args.board == 'lcd':
        default_files.append(root / 'tools/ci' / f'lcd-{args.flavour}.defaults')
        if args.profile == 'stable':
            default_files.append(root / 'sdkconfig.stable.defaults')
    defaults = ';'.join(str(p) for p in default_files)
    subprocess.run([sys.executable, str(Path(idf) / 'tools/idf.py'), '-B', str(build),
                    '-D', f'SDKCONFIG={build / "sdkconfig"}', '-D', f'SDKCONFIG_DEFAULTS={defaults}',
                    '-D', f'IDF_TARGET={target}', 'build'], cwd=project, check=True)
    # Existing build directories retain sdkconfig. Reject accidental flavour drift.
    config = set((build / 'sdkconfig').read_text(encoding='utf-8').splitlines())
    expected_debug = args.flavour == 'debug'
    options = ['REMOTE_DBG_SIM']
    if args.board == 'lcd':
        if args.profile == 'stable' and not {
                'CONFIG_ESPTOOLPY_FLASHFREQ_80M=y', 'CONFIG_SPIRAM_SPEED_80M=y'} <= config:
            raise RuntimeError('stable profile clock drift; refresh its build sdkconfig')
        options.append('APP_DEBUG_FAULT_INJECTION')
    for option in options:
        if (f'CONFIG_{option}=y' in config) != expected_debug:
            raise RuntimeError(f'{option} does not match {args.flavour}; check {build / "sdkconfig"}')
    name = 'esp32_camera_remote' if args.board == 'lcd' else 'm5_atom_matrix'
    if args.board == 'atom' and not {
            'CONFIG_BTDM_CTRL_MODE_BTDM=y', 'CONFIG_BT_BLE_ENABLED=y',
            'CONFIG_BT_GATTC_ENABLE=y'} <= config:
        raise RuntimeError('ATOM BLE support disabled; refresh its build sdkconfig')
    if args.board == 'lcd':
        subprocess.run([sys.executable, str(root / 'tools/check_component_graph.py'), str(build),
                        '--json', str(build / 'component-graph.json')], check=True)
        component_nm = shutil.which('xtensa-esp-elf-nm')
        if not component_nm:
            parser.error('xtensa-esp-elf-nm not found in exported IDF PATH')
        subprocess.run([sys.executable, str(root / 'tools/check_module_symbols.py'), str(build),
                        '--nm', component_nm, '--json', str(build / 'module-symbol-edges.json')], check=True)
        if 'CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y' not in config:
            raise RuntimeError('LCD OTA rollback is disabled; refresh the build sdkconfig')
        if (build / f'{name}.bin').stat().st_size > 5 * 1024 * 1024:
            raise RuntimeError('LCD image exceeds 5 MiB OTA budget')
    subprocess.run([sys.executable, '-m', 'esp_idf_size', '--format', 'json',
                    '-o', str(build / 'size.json'), str(build / f'{name}.map')], check=True)
    if args.flavour == 'release':
        nm = shutil.which('xtensa-esp-elf-nm')
        if not nm:
            parser.error('xtensa-esp-elf-nm not found in exported IDF PATH')
        elf = build / f'{name}.elf'
        symbols = subprocess.check_output([nm, '--defined-only', str(elf)], text=True)
        names = {line.split()[-1] for line in symbols.splitlines() if line.split()}
        forbidden = {'pad_cmd_parse', 'pad_player_tick', 'atom_sim_transact',
                     'matrix_debug_frame', 'atom_fault_take'}
        if args.board == 'lcd':
            forbidden.update({'jpeg_enc_open', 'jpeg_enc_process', 'app_ui_test_jpeg',
                              'input_sim_start', 'input_sim_stop', 'lcd_sim_command',
                              'lcd_sim_enabled', 'lcd_sim_event', 'lcd_sim_poll',
                              'display_bench_command', 'display_bench_event', 'display_bench_poll',
                              'ui_bench_message', 'ui_bench_quiesce', 'camera_display_begin',
                              'camera_display_ready', 'camera_display_end', 'app_ui_test_display_fault'})
        present = names & forbidden
        if present:
            raise RuntimeError(f'release contains simulator symbols: {sorted(present)}')
        print('release simulator symbol check passed')


if __name__ == '__main__':
    main()
