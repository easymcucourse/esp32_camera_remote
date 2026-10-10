"""Check current-topic language coverage and English/Chinese/Japanese navigation."""
import argparse
import json
from pathlib import Path

GROUPS = ('design', 'development', 'request', 'tools', 'user-guide')
ORDER = ('English', '简体中文', '日本語')


def check(root):
    issues = []
    manifest_path = root / 'docs/locales.json'
    try:
        manifest = json.loads(manifest_path.read_text(encoding='utf-8'))
    except (OSError, ValueError) as error:
        return 0, [f'docs/locales.json: {error}']
    if manifest.get('default_language') != 'en' or manifest.get('language_order') != ['en', 'zh-CN', 'ja']:
        issues.append('docs/locales.json: expected default en and language order en, zh-CN, ja')
    topics = manifest.get('topics', [])
    if not isinstance(topics, list) or not all(isinstance(topic, str) for topic in topics):
        return 0, issues + ['docs/locales.json: topics must be a list of paths']
    observed = {path.relative_to(root / 'docs').as_posix()
                for group in GROUPS for path in (root / 'docs' / group).glob('*.md')}
    if len(topics) != len(set(topics)) or set(topics) != observed:
        issues.append('docs/locales.json: topics must list every current topic exactly once')
    files = [root / name for name in ('README.md', 'README.zh-CN.md', 'README.ja.md',
             'm5_atom_matrix/README.md', 'm5_atom_matrix/README.zh-CN.md', 'm5_atom_matrix/README.ja.md',
             'docs/README.md', 'docs/README.zh-CN.md', 'docs/en/README.md', 'docs/ja/README.md')]
    for topic in topics:
        if topic not in observed:
            continue
        files.extend(root / 'docs' / prefix / topic for prefix in ('', 'en', 'ja'))
    for file in files:
        relative = file.relative_to(root)
        if not file.is_file():
            issues.append(f'{relative}: missing language document')
            continue
        body = file.read_text(encoding='utf-8-sig')
        lines = body.splitlines()
        nav = next((line for line in lines[:8] if all(label in line for label in ORDER)), '')
        if not nav or [nav.index(label) for label in ORDER] != sorted(nav.index(label) for label in ORDER):
            issues.append(f'{relative}: missing or unordered English/Chinese/Japanese navigation')
        if not lines or not lines[0].startswith('# ') or len(body) < 100:
            issues.append(f'{relative}: missing title or content')
    # This gate proves coverage/navigation, not semantic translation accuracy.
    return len(topics), issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    count, issues = check(args.root.resolve())
    print(f'{count} current topics, 3 languages, {len(issues)} issues')
    for issue in issues:
        print(issue)
    return bool(issues)


if __name__ == '__main__':
    raise SystemExit(main())
