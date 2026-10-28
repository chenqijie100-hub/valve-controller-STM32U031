"""Run manually in a normal local terminal to make the authorized local commits.

Never pushes. Refuses an existing HEAD/index so it cannot absorb other staged
work or rewrite a user's history. Source hashes must match the reviewed delivery.
"""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'tmp/phase2a-baseline-20261028'
REPORT = ROOT / 'STM32_PROJECT/docs/phase2a-changes.json'

def main():
    git = shutil.which('git') or 'D:/Espressif/tools/git/cmd/git.exe'
    def call(*args, capture=True, check=True):
        return subprocess.run([git, '-C', str(ROOT), *args], check=check,
                              text=True, capture_output=capture)
    if Path(call('rev-parse', '--show-toplevel').stdout.strip()).resolve() != ROOT.resolve():
        raise SystemExit('Unexpected Git root; no changes staged.')
    if call('rev-parse', '--verify', 'HEAD', check=False).returncode == 0:
        raise SystemExit('HEAD already exists. Inspect existing commits before proceeding; no changes staged.')
    if call('diff', '--cached', '--name-only').stdout.strip():
        raise SystemExit('Index contains staged files; no changes staged.')
    report = json.loads(REPORT.read_text(encoding='utf-8'))
    for name, expected in report['sha256'].items():
        if hashlib.sha256((ROOT/name).read_bytes()).hexdigest() != expected:
            raise SystemExit(f'Delivery file changed: {name}; no changes staged.')
    old = json.loads((BASE/'sha256.json').read_text(encoding='utf-8'))
    zip_path = BASE/'source.zip'
    if hashlib.sha256(zip_path.read_bytes()).hexdigest() != report['baseline_zip_sha256']:
        raise SystemExit('Baseline ZIP changed; no changes staged.')
    with tempfile.TemporaryDirectory(prefix='phase2a_commit_', dir=ROOT/'tmp') as temp:
        temp_root = Path(temp)
        with zipfile.ZipFile(zip_path) as archive:
            if set(archive.namelist()) != set(old):
                raise SystemExit('Unexpected baseline ZIP entries.')
            for name, expected in old.items():
                target = (temp_root/name).resolve()
                if not target.is_relative_to(temp_root.resolve()):
                    raise SystemExit('Invalid baseline path.')
                data = archive.read(name)
                if hashlib.sha256(data).hexdigest() != expected:
                    raise SystemExit(f'Baseline hash mismatch: {name}')
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        # Stage archived originals without ever replacing working-tree sources.
        baseline_paths = temp_root/'baseline-paths.txt'
        baseline_paths.write_text('\n'.join(sorted(old))+'\n', encoding='utf-8')
        call('--work-tree='+str(temp_root), 'add', '--pathspec-from-file='+str(baseline_paths))
        call('--work-tree='+str(temp_root), 'commit', '-q', '-m',
             'chore: preserve board-validated phase 1 baseline')
        print('Phase 1 baseline:', call('rev-parse', 'HEAD').stdout.strip())
        delivery_paths = temp_root/'phase2a-paths.txt'
        delivery_paths.write_text('\n'.join(report['modified']+report['added'])+'\n', encoding='utf-8')
        call('add', '--pathspec-from-file='+str(delivery_paths))
        call('commit', '-q', '-m', 'feat: add phase 2A battery sampling and status snapshot')
        print('Phase 2A commit:', call('rev-parse', 'HEAD').stdout.strip())
        print('Local commits complete. No Push performed.')

if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        raise SystemExit(error.stderr or str(error))
