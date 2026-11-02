#!/usr/bin/env python3
"""Build the public MyLISP project with Python 3 and z88dk."""
import json
from pathlib import Path
import shutil
import subprocess
import sys


def main():
    root = Path(__file__).resolve().parent
    if not shutil.which('zcc'):
        raise SystemExit('zcc not found. Install z88dk and add its bin directory to PATH.')
    (root / 'build').mkdir(exist_ok=True)
    tasks = json.loads((root / '.vscode/tasks.json').read_text(encoding='utf-8'))['tasks']
    for task in tasks:
        args = [arg.replace('${workspaceFolder}', root.as_posix()).replace('\\', '/')
                for arg in task['args']]
        subprocess.run(['zcc', *args], cwd=root, check=True)
    print('Built:', root / 'build/MyLISP.nex')


if __name__ == '__main__':
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
