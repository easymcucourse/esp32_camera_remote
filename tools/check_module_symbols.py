"""Audit direct project-symbol references in actual built LCD component archives."""
import argparse
import json
from pathlib import Path
import re
import subprocess
from collections import defaultdict
from check_component_graph import ALLOWED, inspect


def public_calls(root, owner):
    result = set()
    for header in (root / 'components' / owner / 'include').glob('*.h'):
        result.update(re.findall(r'\b(app_\w+)\s*\(', header.read_text(encoding='utf-8')))
    return result


def validate(edges, declarations, public):
    for edge in edges:
        caller, provider = edge['caller'], edge['provider']
        if (caller, provider) not in declarations:
            raise ValueError(f'undeclared direct symbol dependency: {caller} -> {provider}')
        if caller == 'esp_http_server':
            if provider != 'wifi_esp32' or set(edge['symbols']) != {'wifi_esp32_http_bind'}:
                raise ValueError('SDK HTTP bypasses its single backend bind adapter')
        elif provider in {'app_camera', 'app_ui'}:
            if caller != 'app_core' or not set(edge['symbols']) <= public[provider]:
                raise ValueError(f'functional/private direct call bypass: {caller} -> {provider}')
        elif provider == 'app_input' and caller != 'app_core':
            if caller not in {'app_input_atom', 'app_input_sim'} or any(
                    not s.startswith('input_provider_') for s in edge['symbols']):
                raise ValueError('provider bypasses the Input report contract')


def audit(build, nm):
    compiled_graph = inspect(build)
    metadata = json.loads((build / 'project_description.json').read_text(encoding='utf-8'))
    root = Path(metadata['project_path']).resolve()
    owners, references = defaultdict(set), {}
    # SDK foundations are outside this direct-project audit, except the SDK
    # HTTP archive which intentionally references the project's bind adapter.
    for component, data in metadata['build_component_info'].items():
        if component not in ALLOWED and component != 'esp_http_server':
            continue
        archive = data.get('file')
        if not archive:  # Release config-only SIM has no archive.
            continue
        output = subprocess.check_output([nm, '-g', '--defined-only', archive], text=True)
        for line in output.splitlines():
            fields = line.split()
            if len(fields) >= 3 and len(fields[-2]) == 1:
                owners[fields[-1]].add(component)
        output = subprocess.check_output([nm, '-u', archive], text=True)
        references[component] = {line.split()[-1] for line in output.splitlines()
                                 if len(line.split()) >= 2 and line.split()[-2] == 'U'}
    direct = defaultdict(set)
    for caller, symbols in references.items():
        for symbol in symbols:
            for provider in owners.get(symbol, ()):
                if caller != provider:
                    direct[caller, provider].add(symbol)
    edges = [{'caller': c, 'provider': p, 'symbols': sorted(symbols)}
             for (c, p), symbols in sorted(direct.items())]
    declarations = {(c, p) for c, p, _ in compiled_graph['edges']}
    public = {n: public_calls(root, n) for n in ('app_camera', 'app_ui')}
    validate(edges, declarations, public)
    return edges


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--nm', required=True)
    parser.add_argument('--json', type=Path)
    args = parser.parse_args()
    edges = audit(args.build.resolve(), args.nm)
    if args.json:
        args.json.write_text(json.dumps(edges, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'{len(edges)} direct project symbol edges: declarations and lifecycle/provider boundaries passed')


if __name__ == '__main__':
    main()
