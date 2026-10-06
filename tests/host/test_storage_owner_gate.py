"""Ensure direct LCD NVS bypasses cannot silently pass the owner gate."""
from pathlib import Path
import sys
import tempfile

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from check_module_boundaries import storage_writer_issues


with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    for owner in ('wifi_esp32/wifi_saved_config.c',
                  'common_runtime/camera_identity_store.c',
                  'common_runtime/preferences_store.c'):
        source = root / 'components' / owner
        source.parent.mkdir(parents=True, exist_ok=True)
        source.write_text('void save(void) { nvs_set_blob(h,k,p,n); nvs_commit(h); }', encoding='utf-8')
    assert not storage_writer_issues(root)
    bypass = root / 'components/app_ui/write.c'
    bypass.parent.mkdir(parents=True)
    bypass.write_text('void write(void) { nvs_set_u8(h,k,1); }', encoding='utf-8')
    assert len(storage_writer_issues(root)) == 1
    bypass.write_text('/* nvs_set_u8(h,k,1); */\n// nvs_commit(h);\n', encoding='utf-8')
    assert not storage_writer_issues(root)
    bypass.write_text('void erase(void) { nvs_erase_all(h); }', encoding='utf-8')
    assert len(storage_writer_issues(root)) == 1
    bypass.write_text('', encoding='utf-8')
    owner = root / 'components/common_runtime/preferences_store.c'
    owner.write_text('void erase(void) { nvs_flash_erase_partition("nvs"); }', encoding='utf-8')
    assert 'global NVS erase' in storage_writer_issues(root)[0]
    bypass = root / 'common/shortcut.c'
    bypass.parent.mkdir()
    bypass.write_text('void commit(void) { nvs_commit(h); }', encoding='utf-8')
    assert len(storage_writer_issues(root)) == 2
