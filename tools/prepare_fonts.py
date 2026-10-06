"""Fetch pinned upstream fonts and generate OFL-compliant firmware subsets.

Requires fonttools. Original downloads are cached in ignored .reference/fonts.
"""
import hashlib
import json
from pathlib import Path
from urllib.request import Request, urlopen
from zipfile import ZipFile

from fontTools import subset
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
CACHE = ROOT / '.reference' / 'fonts'
ASSETS = ROOT / 'components' / 'app_ui' / 'fonts'


def fetch(url, name):
    path = CACHE / name
    if not path.exists():
        request = Request(url, headers={'User-Agent': 'esp32-camera-remote-fonts'})
        with urlopen(request, timeout=120) as response:
            path.write_bytes(response.read())
    return path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(source, destination, family, characters):
    font = TTFont(source, recalcTimestamp=False)
    available = set(font.getBestCmap())
    options = subset.Options()
    options.layout_features = ['kern']
    options.name_IDs = ['*']
    options.name_legacy = True
    options.name_languages = ['*']
    options.notdef_glyph = True
    options.notdef_outline = True
    options.recommended_glyphs = True
    subsetter = subset.Subsetter(options=options)
    subsetter.populate(unicodes=sorted(characters & available))
    subsetter.subset(font)
    # Modified subsets use new family names to respect reserved font names.
    for name in font['name'].names:
        replacement = {1: family, 2: 'Regular', 3: family + '-Regular',
                       4: family + ' Regular', 6: family.replace(' ', '') + '-Regular',
                       16: family, 17: 'Regular'}.get(name.nameID)
        if replacement:
            name.string = replacement.encode(name.getEncoding())
    if 'CFF ' in font:
        cff = font['CFF '].cff
        cff.fontNames = [family.replace(' ', '') + '-Regular']
        cff.topDictIndex[0].FamilyName = family
        cff.topDictIndex[0].FullName = family + ' Regular'
    output = ASSETS / destination
    font.save(output)
    count = len(font.getBestCmap())
    font.close()
    print(f'{destination}: {output.stat().st_size:,} bytes, {count} characters')
    return {'file': destination, 'family': family, 'sha256': sha256(output),
            'source_sha256': sha256(source), 'bytes': output.stat().st_size,
            'characters': count}


def main():
    CACHE.mkdir(parents=True, exist_ok=True)
    ASSETS.mkdir(parents=True, exist_ok=True)
    inter_url = 'https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip'
    inter_zip = fetch(inter_url, 'Inter-4.1.zip')
    inter = CACHE / 'Inter-Regular.ttf'
    with ZipFile(inter_zip) as archive:
        name = next(n for n in archive.namelist() if n.endswith('extras/ttf/Inter-Regular.ttf'))
        inter.write_bytes(archive.read(name))
        license_name = next(n for n in archive.namelist() if n.endswith('LICENSE.txt'))
        (ASSETS / 'Inter-LICENSE.txt').write_bytes(archive.read(license_name))

    source_url = ('https://raw.githubusercontent.com/adobe-fonts/source-han-sans/'
                  '2.005R/OTF/SimplifiedChinese/SourceHanSansSC-Regular.otf')
    han = fetch(source_url, 'SourceHanSansSC-Regular-2.005.otf')
    mono_url = ('https://raw.githubusercontent.com/JetBrains/JetBrainsMono/'
                'v2.304/fonts/ttf/JetBrainsMono-Regular.ttf')
    mono = fetch(mono_url, 'JetBrainsMono-Regular-2.304.ttf')
    for url, name in [
        ('https://raw.githubusercontent.com/adobe-fonts/source-han-sans/2.005R/LICENSE.txt',
         'SourceHanSans-LICENSE.txt'),
        ('https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/OFL.txt',
         'JetBrainsMono-LICENSE.txt'),
    ]:
        (ASSETS / name).write_bytes(fetch(url, name).read_bytes())

    latin = set(range(0x20, 0x100)) | {0xFFFD, 0x00B0, 0x2026, 0x2013, 0x2014}
    chinese = set(latin)
    for high in range(0xA1, 0xF8):
        for low in range(0xA1, 0xFF):
            try:
                chinese.update(map(ord, bytes([high, low]).decode('gb2312')))
            except UnicodeDecodeError:
                pass
    manifest = [
        generate(inter, 'inter_ui.ttf', 'Camera UI Sans', latin),
        generate(han, 'han_ui.otf', 'Camera UI Han', chinese),
        generate(mono, 'mono_ui.ttf', 'Camera UI Mono', latin),
    ]
    for entry, url in zip(manifest, [inter_url, source_url, mono_url]):
        entry['source_url'] = url
    (ASSETS / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
