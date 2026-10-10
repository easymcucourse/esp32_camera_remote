"""Check repository Markdown file links and heading fragments (offline)."""
import argparse
from pathlib import Path
import re
import unicodedata
from urllib.parse import unquote, urlsplit


def content(path):
    text = path.read_text(encoding='utf-8-sig')
    # Ignore fenced examples, including tilde fences and indentation.
    lines, fence = [], None
    for line in text.splitlines():
        match = re.match(r'^\s{0,3}(`{3,}|~{3,})', line)
        if match:
            marker = match[1]
            if fence is None:
                fence = marker
            elif marker[0] == fence[0] and len(marker) >= len(fence):
                fence = None
            continue
        if fence is None:
            lines.append(line)
    return '\n'.join(lines)


def anchors(path):
    result, counts = set(), {}
    for heading in re.findall(r'^ {0,3}#{1,6}\s+(.+?)\s*#*$', content(path), re.M):
        heading = re.sub(r'\[([^]]+)\]\([^)]*\)', r'\1', heading).lower()
        slug = ''.join(c for c in heading if c in '-_ ' or unicodedata.category(c)[0] in 'LN').replace(' ', '-')
        number = counts.get(slug, 0)
        counts[slug] = number + 1
        result.add(slug if not number else f'{slug}-{number}')
    result.update(re.findall(r'<(?:a|span)\b[^>]*\bid=["\']([^"\']+)', content(path)))
    return result


def check(root):
    files = sorted({root / 'README.md', root / 'm5_atom_matrix/README.md',
                    *root.glob('README*.md'), *(root / 'm5_atom_matrix').glob('README*.md'),
                    *(root / 'docs').rglob('*.md')})
    issues, checked, cache = [], 0, {}
    for path in files:
        if not path.is_file():
            issues.append(f'{path.relative_to(root)}: missing document')
            continue
        for match in re.finditer(r'\[[^]\n]*\]\(([^)\n]*)\)', content(path)):
            ref = match[1].split(' "')[0].strip('<>')
            if urlsplit(ref).scheme or ref.startswith('//'):
                continue
            filename, _, fragment = ref.partition('#')
            filename, fragment = unquote(filename), unquote(fragment)
            target = (path.parent / filename).resolve() if filename else path
            checked += 1
            if not target.exists():
                issues.append(f'{path.relative_to(root)}: missing file {ref}')
            elif fragment and target.suffix.lower() == '.md':
                if target not in cache:
                    cache[target] = anchors(target)
                if fragment not in cache[target]:
                    issues.append(f'{path.relative_to(root)}: missing anchor {ref}')
    return len(files), checked, issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    documents, links, issues = check(args.root.resolve())
    print(f'{documents} documents, {links} local links, {len(issues)} issues')
    for issue in issues:
        print(issue)
    return bool(issues)


if __name__ == '__main__':
    raise SystemExit(main())
