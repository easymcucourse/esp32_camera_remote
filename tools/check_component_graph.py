"""Verify the LCD's actual IDF graph, including its post-project HTTP link edge."""
import argparse
import json
from pathlib import Path


ALLOWED = {
    'main': {'app_core'},
    'app_core': {'app_wifi', 'common_runtime', 'app_maintenance', 'app_input_atom',
                 'app_input_sim', 'app_input', 'app_camera', 'app_console',
                 'app_ui', 'app_wifi_messages', 'wifi_esp32'},
    'app_camera': {'camera_backend', 'camera_backend_sony', 'app_console', 'common_runtime'},
    'app_console': {'common_runtime'},
    'app_input': {'app_console'},
    'app_input_atom': {'app_input', 'app_console', 'atom_protocol'},
    'app_input_sim': {'app_input', 'app_console', 'atom_protocol'},
    'app_maintenance': {'app_wifi', 'common_runtime'},
    'app_ui': {'app_console', 'common_runtime', 'display_surface'},
    'app_wifi': {'common_runtime'},
    'app_wifi_messages': {'app_wifi', 'app_console', 'common_runtime'},
    'atom_protocol': set(), 'board_7b': set(), 'camera_backend': set(),
    'camera_backend_sony': {'camera_backend', 'ptpip'},
    'common_runtime': set(), 'display_surface': {'board_7b'},
    'ptpip': {'app_console'}, 'wifi_esp32': {'app_wifi', 'common_runtime'},
}


def inspect(build):
    description = json.loads((build / 'project_description.json').read_text(encoding='utf-8'))
    root = Path(description['project_path']).resolve()
    metadata = description['build_component_info']
    local = {name: data for name, data in metadata.items()
             if Path(data['dir']).resolve() == root / 'main' or
             Path(data['dir']).resolve().parent == root / 'components'}
    if set(local) - ALLOWED.keys():
        raise ValueError(f'unreviewed project components: {sorted(set(local) - ALLOWED.keys())}')
    required = set(ALLOWED) - {'app_input_sim'}
    if required - local.keys():
        raise ValueError(f'missing production components: {sorted(required - local.keys())}')
    edges = []
    for owner, data in sorted(local.items()):
        for visibility, key in [('public', 'reqs'), ('private', 'priv_reqs')]:
            for dependency in data.get(key, []):
                if dependency in local and dependency not in ALLOWED[owner]:
                    raise ValueError(f'functional dependency bypass: {owner} -> {dependency}')
                if owner == 'app_core' and visibility == 'public':
                    raise ValueError('Core public startup contract exports implementation dependency')
                if owner == 'app_camera' and dependency in {'camera_backend', 'camera_backend_sony'} and visibility != 'private':
                    raise ValueError('Camera backend dependency is public')
                edges.append((owner, dependency, visibility))
    commands = json.loads((build / 'compile_commands.json').read_text(encoding='utf-8'))
    http_sources = {Path(p).resolve() for p in metadata['esp_http_server']['sources']}
    http_commands = [c for c in commands if Path(c['file']).resolve() in http_sources]
    if not http_commands or any('lwip_bind=wifi_esp32_http_bind' not in c.get('command', '') for c in http_commands):
        raise ValueError('actual HTTP compilation does not use the SoftAP bind adapter')
    link_interface = (build / 'module-http-links.txt').read_text(encoding='utf-8')
    if '__idf_wifi_esp32' not in link_interface.strip().split(';'):
        raise ValueError('actual post-project HTTP link interface misses Wi-Fi backend')
    edges.append(('esp_http_server', 'wifi_esp32', 'post-project'))
    # SDK internals are foundation nodes, not project modules. Check all project
    # paths plus the one SDK node which has an injected project dependency.
    nodes = set(local) | {'esp_http_server'}
    adjacency = {n: [] for n in nodes}
    for owner, dependency, _ in edges:
        if dependency in nodes:
            adjacency[owner].append(dependency)
    complete, visiting = set(), []
    def visit(node):
        if node in visiting:
            raise ValueError('dependency cycle: ' + ' -> '.join(visiting + [node]))
        if node in complete:
            return
        visiting.append(node)
        for child in adjacency[node]:
            visit(child)
        visiting.pop()
        complete.add(node)
    for node in sorted(nodes):
        visit(node)
    release = '#define CONFIG_REMOTE_DBG_SIM 1' not in (build / 'config/sdkconfig.h').read_text(encoding='utf-8')
    sim_compiled = any(Path(c['file']).name == 'input_sim.c' for c in commands)
    if release and (sim_compiled or local.get('app_input_sim', {}).get('sources')):
        raise ValueError('Release compiles SIM implementation code')
    if not release and not sim_compiled:
        raise ValueError('Debug graph misses its required SIM implementation')
    # IDF may retain an empty registered component in Release metadata; this
    # does not mean its implementation was compiled or linked into the image.
    return {'project_nodes': sorted(nodes), 'edges': sorted(edges), 'release': release,
            'empty_components': sorted(n for n, d in local.items() if not d.get('sources'))}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('build', type=Path)
    parser.add_argument('--json', type=Path)
    parser.add_argument('--markdown', type=Path)
    args = parser.parse_args()
    graph = inspect(args.build.resolve())
    if args.json:
        args.json.write_text(json.dumps(graph, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    if args.markdown:
        lines = ['```mermaid', 'flowchart TD']
        for owner, dependency, visibility in graph['edges']:
            if dependency in graph['project_nodes']:
                lines.append(f'    {owner} -->|{visibility}| {dependency}')
        lines.append('```')
        args.markdown.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f"{len(graph['project_nodes'])} project/bind nodes; {len(graph['edges'])} explicit edges; acyclic; HTTP adapter compiled and linked")


if __name__ == '__main__':
    main()
